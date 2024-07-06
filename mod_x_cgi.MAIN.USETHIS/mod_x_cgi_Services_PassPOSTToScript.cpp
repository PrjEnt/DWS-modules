// *
// * XCGI Module (mod_x_cgi) :: Another implementation of 'mod_cgi' for Windows Embedded 7 Standard and later Windows / Windows Server
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *
#include "mod_x_cgi.h"

inline int PassPOSTToScript(request_rec *r, HANDLE* HStdIn){
	apr_bucket *b;
	apr_bucket_brigade *bb;

	char* Block;
	int BlockSize;

	int R;

	bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
	R = ap_get_brigade(r->input_filters, bb, AP_MODE_READBYTES, APR_BLOCK_READ, HUGE_STRING_LEN);

	if (R != APR_SUCCESS) {
		if (APR_STATUS_IS_TIMEUP(R)) {
			return HTTP_REQUEST_TIME_OUT;
		}
		return 500;
	}

	for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = APR_BUCKET_NEXT(b)){
		apr_size_t BlockSize;

		if (APR_BUCKET_IS_EOS(b)) { break; }
		if (APR_BUCKET_IS_FLUSH(b)){ continue; }

		apr_bucket_read(b, ((const char **)(&Block)), &BlockSize, APR_BLOCK_READ);
		if( ! WriteFile(HStdIn[Pipe_Self], Block, BlockSize, ((DWORD*)&R), 0) ){
			Log(r, APLOG_WARNING, "mod_x_cgi", "Failed to write STDIN (%i).", GetLastError());
			break;
		}
	}

	return 0;
}