// *
// * XCGI Module (mod_x_cgi) :: Another implementation of 'mod_cgi' for Windows Embedded 7 Standard and later Windows / Windows Server
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *
#include "mod_x_cgi.h"

inline int PassContentsToClient(request_rec *r, void* Data, int DataSize, HANDLE* HStdOut, OutputHeaderResults *OHR){
	//Structs
	apr_bucket_alloc_t *ba;
	apr_bucket_brigade *bb;
	apr_bucket *b;


	//Initialize Bridge
	bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
	ba = apr_bucket_alloc_create(r->pool);



	//Read STDOUT & Add Data Bucket
	while(1){
		b = apr_bucket_heap_create( ( ((const char*)Data)+OHR->SkipSize ), (OHR->ReadSize - OHR->SkipSize), 0, ba);

		APR_BRIGADE_INSERT_TAIL(bb, b);
		OHR->SkipSize = 0;

		//Data = malloc(DataSize);

		if( ! ReadFile(HStdOut[0], Data, DataSize-1, ( (DWORD*)(&(OHR->ReadSize)) ), 0) ){
			//IO Error: May be closed pipe or critical
			break;
		}
	}


	//Send: End of struct
	b = apr_bucket_eos_create(r->connection->bucket_alloc);
	APR_BRIGADE_INSERT_TAIL(bb, b);

	//Pass: to output filters
	ap_pass_brigade(r->output_filters, bb);

	return 0;
}