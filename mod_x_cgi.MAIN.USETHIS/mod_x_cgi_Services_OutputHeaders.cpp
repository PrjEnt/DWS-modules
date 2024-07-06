// *
// * XCGI Module (mod_x_cgi) :: Another implementation of 'mod_cgi' for Windows Embedded 7 Standard and later Windows / Windows Server
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *
#include "mod_x_cgi.h"

inline int OutputHeaders(request_rec *r, void* Data, int DataSize, HANDLE *HStdOut, OutputHeaderResults* OHR){
	int R = 0;

	HeaderBlockManager HBM = {0};
	apr_table_t *header_out;

	header_out = apr_table_make(r->pool, 8);
	r->headers_out = header_out;

	HBM.t = header_out;
	HBM.Block = Data;
	HBM.BlockSize = DataSize;

	while(1){
		if( ReadFile(HStdOut[Pipe_Self], Data, DataSize, (DWORD*)&(HBM.ReadSize), 0) ){
//Log(r, APLOG_ALERT, "mod_x_cgi", "ReadExe [%i]: %s", DataSize, Data);
			R = AddHeadersFromBlock(&HBM);
			if(R != -1){ //EOH detected (-1: more data required / 0+: EOF detected in this position)
				break;
			}
			if(DataSize < R){
				Log(r, APLOG_ALERT, "mod_x_cgi", "AddHeaders reported current size of Header block is %i, but current DataSize = %i (corrupted?)", R, DataSize);
				return 500;
			}
		} else { //IO Error: May be closed pipe or critical
			R = 0;
			break;
		}
	}
	OHR->SkipSize = R + 1; //+1: final LF
	OHR->ReadSize = HBM.ReadSize;
	r->content_type = apr_table_get(header_out, "Content-Type");

	const char* Status = apr_table_get(header_out, "Status");
	if(Status != 0){
		r->status = AtoI(Status);
	}
	return 0;
}