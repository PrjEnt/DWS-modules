// *
// * Raw Cache
// * (C)2018-2023 Project Entertainments*
// * http://n.xprj.net/
// *
#include "../Include/Apache24Modules.h"
#include <Windows.h>

extern module AP_MODULE_DECLARE_DATA x_rawcache_module;

typedef struct _XRawCacheConfig {
	int  Enabled;
	int  IgnoreHost;
	char CacheDir[256];
} XRawCacheConfig;

typedef enum _XRawCacheResponseState {
	XRRS_Uninitialized = 0,
	XRRS_InProgress,
	XRRS_InProgressByOtherSession,
	XRRS_AlreadyDone,
	XRRS_UnableToCache,

} XRawCacheResponseState;

typedef struct _XRawCacheResponse {
	XRawCacheResponseState State;
	HANDLE		       File;
	char		      *CacheFile;
	char		      *ProgressFile;
	int		       Size;
} XRawCacheResponse, *PXRawCacheResponse;

inline int AtoI(const void *m) {
	const char *s = (const char *) m;
	int	    r = 0;
	while(true) {
		if(*s >= '0' && *s <= '9') {
			r = (int) (r * 10) + (*s - '0');
			s++;
		} else {
			break;
		}
	}
	return r;
}

void *memset4(void *m, int value, int size) {
	int size4 = size >> 2;

	//size is NOT multiple of 4
	if((size4 << 2) != size) { return memset(m, value, size); }

	int *mi = (int *) m;
	for(int i = 0; i < size4; i++) { mi[i] = value; }
	return m;
}

int IsExistFile(const char *Path) {
	if(Path == 0) { return 0; }

	int Flags = GetFileAttributesA(Path);
	if(Flags == INVALID_FILE_ATTRIBUTES) { return 0; }
	if(Flags & FILE_ATTRIBUTE_DIRECTORY) { return 0; }

	return 1;
}

int GetFileSizeByHandle(HANDLE H) {
	int S = SetFilePointer(H, 0, 0, FILE_BEGIN);
	int E = SetFilePointer(H, 0, 0, FILE_END);
	return (E - S);
}

int MakeDirectoryForThisFile(char *FilePath, request_rec *r) {
	int R = 0;
	for(int i = 0; i < 4096; i++) {
		switch(FilePath[i]) {
			//End
			case 0x00: return R;
			case '/':
				FilePath[i] = 0x00;
				R	    = CreateDirectoryA(FilePath, 0);
				FilePath[i] = '\\';
				break;
		}
	}
	return 0;
}

static int x_handler(request_rec *r) {
	XRawCacheConfig *Config = (XRawCacheConfig *) ap_get_module_config(r->per_dir_config, &x_rawcache_module);

	/*
	ap_set_content_type(r, "text/plain");
	ap_rprintf(r, "Enabled: %u\n", Config->Enabled);
	ap_rprintf(r, "Path: %s\n", Config->CacheDir);
	return OK;
	*/

	//Handler
	if(Config->Enabled != 1) { return DECLINED; } //Fast decline

	apr_table_unset(r->headers_in, "Accept-Encoding");
	ap_add_output_filter("XRawCacheSave", 0, r, r->connection);
	return DECLINED;
}

typedef struct _FILE_DISPOSITION_INFO_EX {
	int Flags;
} FILE_DISPOSITION_INFO_EX;

static apr_status_t XRawCacheSaveEx(PXRawCacheResponse RCR, ap_filter_t *f, apr_bucket_brigade *in_bb) {
	request_rec	*r	= f->r;
	XRawCacheConfig *Config = (XRawCacheConfig *) ap_get_module_config(r->per_dir_config, &x_rawcache_module);

	//Nothing to do
	switch(RCR->State) {
		case XRRS_AlreadyDone:
		case XRRS_InProgressByOtherSession:
		case XRRS_UnableToCache: return OK;
	}
	if(r->status != 200) {
		//ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: XRRS_Not200: %s", r->uri);
		apr_table_set(r->headers_out, "X-RawCache", "Not200");
		return OK;
	}

	//Create new cache file
	if(RCR->State == XRRS_Uninitialized) {
		//Make path from URL

		char *CacheDir = Config->CacheDir;
		if(CacheDir[0] == 0) { CacheDir = "../Cache"; }

		const char *HostName = r->hostname;
		if(Config->IgnoreHost == 1){
			HostName = ".";
		}

		char *FilePath = apr_psprintf(f->r->pool, "%s/%s/%s", CacheDir, HostName, r->uri);

		MakeDirectoryForThisFile(FilePath, r);

		char *ProgressPath = apr_psprintf(f->r->pool, "%s.xrawcache", FilePath);
		char *NoCachePath  = apr_psprintf(f->r->pool, "%s.xrawnocache", FilePath);

		//ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: FilePath: %s", FilePath);
		//ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 200, r, "XRawCacheSave: ProgressPath: %s", ProgressPath);

		if(IsExistFile(NoCachePath)) {
			RCR->State = XRRS_UnableToCache;
			apr_table_set(r->headers_out, "X-RawCache", "CacheBlocked");
			//ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_INFO, 200, r, "XRawCacheSave: XRRS_NoCache: %s", RCR->CacheFile);
			return OK;
		}

		RCR->CacheFile	  = FilePath;
		RCR->ProgressFile = ProgressPath;

		//Check Cached file
		if(IsExistFile(RCR->CacheFile)) {
			RCR->State = XRRS_AlreadyDone;
			apr_table_set(r->headers_out, "X-RawCache", "AlreadyCached");
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_INFO, 200, r, "XRawCacheSave: XRRS_AlreadyDone: %s", RCR->CacheFile);
			return OK;
		}

		//Unsupported
		const char *SizeS = apr_table_get(r->headers_out, "Content-Length");

		if(SizeS == 0) {
			RCR->State = XRRS_UnableToCache;
			apr_table_set(r->headers_out, "X-RawCache", "Unsupported");
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_ERR, 200, r, "XRawCacheSave: XRRS_UnableToCache: (No 'Content-Length'): %s", RCR->CacheFile);
			return OK;
		}
		RCR->Size = AtoI(SizeS);

		RCR->State = XRRS_InProgress;

		RCR->File = CreateFileA(RCR->ProgressFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, 0);

		if(RCR->File == INVALID_HANDLE_VALUE) {
			if(GetLastError() == ERROR_SHARING_VIOLATION) {
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
	if(RCR->File == 0) {
		RCR->State = XRRS_UnableToCache;
		int R	   = 501;
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_CRIT, R, r, "XRawCacheSave: File handler is NULL");
		return R;
	}

	int	    R;
	apr_size_t  len;
	const char *data;
	apr_bucket *b = APR_BRIGADE_FIRST(in_bb);
	for(b = APR_BRIGADE_FIRST(in_bb); b != APR_BRIGADE_SENTINEL(in_bb); b = APR_BUCKET_NEXT(b)) {
		if(APR_BUCKET_IS_EOS(b)) {
			int Size = GetFileSizeByHandle(RCR->File);
			CloseHandle(RCR->File);
			//ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 200, r, "XRawCacheSave: EOS!: %s", RCR->ProgressFile);

			if(RCR->Size == Size) {
				MoveFile(RCR->ProgressFile, RCR->CacheFile);
			} else {
				ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_CRIT, 200, r, "XRawCacheSave: File size missmatch. Not saved. Content-Length: %i / File: %i: %s", RCR->Size, Size, RCR->CacheFile);
				DeleteFile(RCR->ProgressFile);
			}

			return OK;
		}
		apr_bucket_read(b, &data, &len, APR_BLOCK_READ);
		WriteFile(RCR->File, data, len, (DWORD *) &R, 0);
		//ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 200, r, "WriteFile: %i", R);
	}

	return OK;
}

static apr_status_t XRawCacheSave(ap_filter_t *f, apr_bucket_brigade *in_bb) {
	if(APR_BRIGADE_EMPTY(in_bb)) { return APR_SUCCESS; }

	if(f->ctx == 0) { //First call per request
		//apr_palloc: Already '0' cleared.
		f->ctx = apr_palloc(f->r->pool, sizeof(XRawCacheResponse));
		memset4(f->ctx, 0x00, sizeof(XRawCacheResponse));
	}

	//Unexpected value
	PXRawCacheResponse RCR = (PXRawCacheResponse) f->ctx;
	if(RCR == 0) {
		int R = 501;
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO | APLOG_CRIT, R, f->r, "XRawCacheSave: ctx is NULL. It may be insufficient memory.");
		return R;
	}

	apr_status_t R = XRawCacheSaveEx(RCR, f, in_bb);

	if(R == OK) {
		return ap_pass_brigade(f->next, in_bb);
	} else {
		f->r->status = R;
		CloseHandle(RCR->File);
	}
	return R;
}

const char *x_rawcache_enable(cmd_parms *cmd, void *cfg, int Flag) {
	if(cfg == 0) { return 0; }

	XRawCacheConfig *Config = (XRawCacheConfig *) cfg;
	Config->Enabled		= Flag;
	return 0;
}

const char *x_rawcache_ignore_host(cmd_parms *cmd, void *cfg, int Flag) {
	if(cfg == 0) { return 0; }

	XRawCacheConfig *Config = (XRawCacheConfig *) cfg;
	Config->IgnoreHost	= Flag;
	return 0;
}

const char *x_rawcache_dir(cmd_parms *cmd, void *cfg, const char *arg) {
	if(cfg == 0) { return 0; }

	XRawCacheConfig *Config = (XRawCacheConfig *) cfg;
	strcpy(Config->CacheDir, arg);
	return 0;
}

static const command_rec x_rawcache_directives[] = {
	{ "XRawCacheEnable", (cmd_func) x_rawcache_enable, 0, OR_OPTIONS, FLAG, "" },
	{ "XRawCacheIgnoreHost", (cmd_func) x_rawcache_ignore_host, 0, OR_OPTIONS, FLAG, "" },
	{ "XRawCacheDir", (cmd_func) x_rawcache_dir, 0, OR_OPTIONS, TAKE1, "" },

 //AP_INIT_FLAG("XRawCacheEnable", x_rawcache_enable, 0, OR_OPTIONS, ""),
	//AP_INIT_TAKE1("XRawCacheDir", x_rawcache_dir, 0, OR_OPTIONS, ""),

	{ 0 }
};

void *create_dir_conf(apr_pool_t *pool, char *context) {
	context			= context ? context : "New";
	XRawCacheConfig *Config = (XRawCacheConfig *) apr_pcalloc(pool, sizeof(XRawCacheConfig));

	if(Config == 0) { return 0; }

	Config->Enabled	    = -1;
	Config->IgnoreHost  = -1;
	Config->CacheDir[0] = 0;

	return Config;
}

void *merge_dir_conf(apr_pool_t *pool, void *BasePtr, void *AddPtr) {
	XRawCacheConfig *Base	= (XRawCacheConfig *) BasePtr;
	XRawCacheConfig *Add	= (XRawCacheConfig *) AddPtr;
	XRawCacheConfig *Merged = (XRawCacheConfig *) create_dir_conf(pool, "Merged");

	memcpy(Merged, Base, sizeof(XRawCacheConfig));

	if(Add->Enabled != -1) { Merged->Enabled = Add->Enabled; }
	if(Add->IgnoreHost != -1) { Merged->IgnoreHost = Add->IgnoreHost; }

	if(Add->CacheDir[0] != 0) { memcpy(Merged->CacheDir, Add->CacheDir, sizeof(Merged->CacheDir)); }

	return Merged;
}

static void register_hooks(apr_pool_t *p) {
	printf("I: mod_x_rawcache: Loaded\n");

	ap_hook_handler(x_handler, 0, 0, APR_HOOK_REALLY_FIRST);
	ap_register_output_filter("XRawCacheSave", XRawCacheSave, 0, AP_FTYPE_RESOURCE);
}

APLOG_USE_MODULE(x_rawcache);
module AP_MODULE_DECLARE_DATA x_rawcache_module = {
	STANDARD20_MODULE_STUFF, /* */
	create_dir_conf,	 /* create per-directory config structure */
	merge_dir_conf,		 /* merge per-directory config structures */
	0,			 /* create per-server config structure */
	0,			 /* merge per-server config structures */
	x_rawcache_directives,	 /* command apr_table_t */
	register_hooks		 /* register hooks */
};
