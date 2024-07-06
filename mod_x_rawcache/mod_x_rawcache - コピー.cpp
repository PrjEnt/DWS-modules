// *
// * Raw Cache
// * (C)2018-2023 Project Entertainments*
// * http://n.xprj.net/
// *
#include "../Include/Apache24Modules.h"
#include <Windows.h>

typedef enum _XRawCacheResponseState {
	XRRS_Uninitialized = 0,
	XRRS_InProgress,
	XRRS_InProgressByOtherSession,
	XRRS_AlreadyDone,
	XRRS_UnableToCache,

} XRawCacheResponseState;

typedef struct _XRawCacheResponse {
	XRawCacheResponseState State;
	HANDLE File;
	char* CacheFile;
	char* ProgressFile;
	int Size;
} XRawCacheResponse, *PXRawCacheResponse;

inline int AtoI(const void *m){
	const char* s = (const char*) m;
	int r = 0;
	while(true){
		if (*s >= '0' && *s <= '9') {
			r = (int) (r * 10) + (*s - '0');
			s++;
		} else  {	
			break;
		}
	}
	return r;
}

void* memset4(void* m, int value, int size){
	int size4 = size >> 2;

	//size is NOT multiple of 4
	if( (size4 << 2) != size ){
		return memset(m, value, size);
	}

	int* mi = (int*) m;
	for(int i=0; i<size4; i++){
		mi[i] = value;
	}
	return m;
}

int IsExistFile(const char* Path){
	if(Path == 0){
		return 0;
	}

	int Flags = GetFileAttributesA(Path);
	if(Flags == INVALID_FILE_ATTRIBUTES){
		return 0;
	}
	if(Flags & FILE_ATTRIBUTE_DIRECTORY){
		return 0;
	}

	return 1;
}

int GetFileSizeByHandle(HANDLE H){
	int S = SetFilePointer(H, 0, 0, FILE_BEGIN);
	int E = SetFilePointer(H, 0, 0, FILE_END);
	return (E-S);
}

int MakeDirectoryForThisFile(char* FilePath, request_rec *r){
	int R = 0;
	for(int i=0; i<4096; i++){
		switch(FilePath[i]){
			//End
			case 0x00:
				return R;
			case '/':
				FilePath[i] = 0x00;
				R = CreateDirectoryA(FilePath, 0);
				FilePath[i] = '\\';
				break;
		}
	}
	return 0;
}

static int x_handler(request_rec *r) {

	//Handler
	if(r->handler[0] != 'x') { return DECLINED; } //Fast decline
	if(strcmp(r->handler, "x-rawcache")) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: DECLINED");

		return DECLINED;
	}
	ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: ACCEPTED");

	apr_table_unset(r->headers_in, "Accept-Encoding");

	ap_add_output_filter("XRawCacheSave", 0, r, r->connection);
	return DECLINED;
}


typedef struct _FILE_DISPOSITION_INFO_EX {
    int Flags;
} FILE_DISPOSITION_INFO_EX;

static apr_status_t XRawCacheSaveEx(PXRawCacheResponse RCR, ap_filter_t *f, apr_bucket_brigade *in_bb) {
	request_rec* const r = f->r;

		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: StartEx");

	//Nothing to do
	switch(RCR->State){
		case XRRS_AlreadyDone:
		case XRRS_InProgressByOtherSession:
		case XRRS_UnableToCache:
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: AlreadyOrUnable");
			return OK;
	}
	
	if(r->status != 200){
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: XRRS_Not200: %s", r->uri);
		apr_table_set(r->headers_out, "X-RawCache", "Not200");
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: NO200");

		return OK;
	}

		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: CONTINUE");

	

	//Create new cache file
	if(RCR->State == XRRS_Uninitialized){
				ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: UNINIT");

		//Make path from URL
		char* FilePath = apr_psprintf(f->r->pool, "../Cache/%s/%s", r->hostname, r->uri);
		MakeDirectoryForThisFile(FilePath, r);

		char* ProgressPath = apr_psprintf(f->r->pool, "%s.xrawcache", FilePath);
		char* NoCachePath = apr_psprintf(f->r->pool, "%s.xrawnocache", FilePath);

		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 200, r, "XRawCacheSave: FilePath: %s", FilePath);
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 200, r, "XRawCacheSave: ProgressPath: %s", ProgressPath);

		if(IsExistFile(NoCachePath)){
				RCR->State = XRRS_UnableToCache;
				apr_table_set(r->headers_out, "X-RawCache", "CacheBlocked");
				ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_INFO, 200, r, "XRawCacheSave: XRRS_NoCache: %s", RCR->CacheFile);
				return OK;
		}

		RCR->CacheFile = FilePath;
		RCR->ProgressFile = ProgressPath;
		
		//Check Cached file
		if(IsExistFile(RCR->CacheFile)){
				RCR->State = XRRS_AlreadyDone;
				apr_table_set(r->headers_out, "X-RawCache", "AlreadyCached");
				ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_INFO, 200, r, "XRawCacheSave: XRRS_AlreadyDone: %s", RCR->CacheFile);
				return OK;
		}

		//Unsupported
		const char* SizeS = apr_table_get(r->headers_out, "Content-Length");

		if(SizeS == 0){
			RCR->State = XRRS_UnableToCache;
			apr_table_set(r->headers_out, "X-RawCache", "Unsupported");
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: XRRS_UnableToCache: (No 'Content-Length'): %s", RCR->CacheFile);
			return OK;
		}
		RCR->Size = AtoI(SizeS);
		
		RCR->State = XRRS_InProgress;

		RCR->File = CreateFileA(RCR->ProgressFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, 0);

		if(RCR->File == INVALID_HANDLE_VALUE ){

			if(GetLastError() == ERROR_SHARING_VIOLATION){
				RCR->State = XRRS_InProgressByOtherSession;
				apr_table_set(r->headers_out, "X-RawCache", "InProgress");
				ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: XRRS_InProgressByOtherSession: %s", RCR->ProgressFile);
				return OK;
			}
			
			RCR->State = XRRS_UnableToCache;
			apr_table_set(r->headers_out, "X-RawCache", "Unsupported");
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: XRRS_UnableToCache: %s", RCR->ProgressFile);
			return OK;
		}

		apr_table_set(r->headers_out, "X-RawCache", "Cached");
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_INFO, 200, r, "XRawCacheSave: New cache session");
	}

	//Unexpected value
	if(RCR->File == 0){
		RCR->State = XRRS_UnableToCache;
		int R = 501;
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_CRIT, R, r, "XRawCacheSave: File handler is NULL");
		return R;
	}


	int R;
	apr_size_t len;
    const char* data;
	apr_bucket *b = APR_BRIGADE_FIRST(in_bb);
	for (b = APR_BRIGADE_FIRST(in_bb); b != APR_BRIGADE_SENTINEL(in_bb); b = APR_BUCKET_NEXT(b)) {
		if( APR_BUCKET_IS_EOS(b) ){

			int Size = GetFileSizeByHandle(RCR->File);
			CloseHandle(RCR->File);
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 200, r, "XRawCacheSave: EOS!: %s", RCR->ProgressFile);

			if(RCR->Size == Size){
				MoveFile(RCR->ProgressFile, RCR->CacheFile);
			} else {
				ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_CRIT, 200, r, "XRawCacheSave: File size missmatch. Not saved. Content-Length: %i / File: %i: %s", RCR->Size, Size, RCR->CacheFile);
				DeleteFile(RCR->ProgressFile);
			}

			return OK;
		}
		apr_bucket_read(b, &data, &len, APR_BLOCK_READ);
		WriteFile(RCR->File, data, len, (DWORD*)&R, 0);
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 200, r, "WriteFile: %i", R);
	}


	return OK;
}

static apr_status_t XRawCacheSave(ap_filter_t *f, apr_bucket_brigade *in_bb) {
	
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, f->r, "XRawCacheSave: Start");

	
	if(APR_BRIGADE_EMPTY(in_bb)){
        return APR_SUCCESS;
	}

	if(f->ctx == 0){ //First call per request
		//apr_palloc: Already '0' cleared.
		f->ctx = apr_palloc(f->r->pool, sizeof(XRawCacheResponse));
		memset4(f->ctx, 0x00, sizeof(XRawCacheResponse));
	}

	//Unexpected value
	PXRawCacheResponse RCR = (PXRawCacheResponse) f->ctx;
	if(RCR == 0){
		int R = 501;
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_CRIT, R, f->r, "XRawCacheSave: ctx is NULL. It may be insufficient memory.");
		return R;
	}

	apr_status_t R = XRawCacheSaveEx(RCR, f, in_bb);

	if(R == OK){
		return ap_pass_brigade(f->next, in_bb);
	} else {
		f->r->status = R;
		CloseHandle(RCR->File);
	}
	return R;
}

static void register_hooks(apr_pool_t *p) { 
	printf("I: mod_x_rawcache.cpp\n");

	ap_hook_handler(x_handler, 0, 0, APR_HOOK_REALLY_FIRST);
	ap_register_output_filter("XRawCacheSave", XRawCacheSave, 0, AP_FTYPE_RESOURCE);
}

AP_DECLARE_MODULE(x_rawcache) = {
	STANDARD20_MODULE_STUFF,
	0,			/* create per-directory config structure */
	0,			/* merge per-directory config structures */
	0,			/* create per-server config structure */
	0,			/* merge per-server config structures */
	0,			/* command apr_table_t */
	register_hooks		/* register hooks */
};
