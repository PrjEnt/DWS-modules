// *
// * XCGI Module (mod_x_cgi) :: Another implementation of 'mod_cgi' for Windows Embedded 7 Standard and later Windows / Windows Server
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *
#include "mod_x_cgi.h"

static int x_handler(request_rec *r){

	char* ENV_SR = 0;

	//Handler
	if( r->handler[0] != 'x'){ return DECLINED; } //Fast decline
	if( strcmp(r->handler, "x-cgi-script") ) {
		if( !strcmp(r->handler, "x-cgi-script-with-sr") ) {
			int size = GetEnvironmentVariable("SystemRoot", 0, 0);
			ENV_SR = (char*) apr_palloc(r->pool, size);
			size = GetEnvironmentVariable("SystemRoot", ENV_SR, size);
			if(size==0){ENV_SR = 0;}
		} else {
			return DECLINED;
		}
	}
	//End of Handler

	//Script 404 checker
	if(r->finfo.filetype == APR_NOFILE) {
		Log(r, APLOG_NOTICE, "mod_x_cgi", "Script file not found [%s]", r->filename);
		return HTTP_NOT_FOUND;
	}
	//End of 404 checker

	//Main Code
	{
		//Values
		int R = 0; //Returned
		int FileNameLastSlash = 0;
		char *ScriptCommand;

		//Buffer
		int DataSize = 16384;
		void* Data =  apr_palloc(r->pool, DataSize+1);
		char* DataS = (char*)Data;

		//Structs
		STARTUPINFO si = {0};
		PROCESS_INFORMATION pi;

		HANDLE HStdOut[2];
		HANDLE HStdIn[2];
		HANDLE HStdErr[2];

		OutputHeaderResults OHR = {0};
		EnvBlockManager EBM = {0};


		//Prepare Process
		R = PreapreProcess(r, &si, HStdOut, HStdIn, HStdErr, &ScriptCommand);
		if(R != 0){
			return R;
		}

		//Divide Directory & FileName from r->filename
		FileNameLastSlash = FindFinal('/', r->filename, MaxMemorySize); //100% null contains by apr. so Memory Size is dummy.

		//Build Env Block
		BuildEnvBlock(r, &EBM, FileNameLastSlash, ENV_SR);

		//Create Process
		R = CreateCGIProcess(r, &si, &pi, ScriptCommand, FileNameLastSlash, &EBM);
		if(R != 0){
			if(R == 200){R=0;}
			return R;
		}

		//When CreateProcess(), Inherited Handles are copied to child process.
		//	So original Handles are not necessary. Close!
		CloseHandle(HStdOut[Pipe_Child]);
		CloseHandle(HStdIn[Pipe_Child]);



		//POST
		if( r->method_number == M_POST ){
			R = PassPOSTToScript(r, HStdIn);
			if(R != 0){
				return R;
			}
			CloseHandle(HStdIn[Pipe_Self]);
			CloseHandle(HStdIn[Pipe_Child]);
		}



		//Header Output
		R = OutputHeaders(r, Data, DataSize, HStdOut, &OHR);
		if(R != 0){
			return R;
		}

		//HEAD or Other
		if (!r->header_only){ //not HEAD method
			PassContentsToClient(r, Data, DataSize, HStdOut, HStdErr, &OHR);
			return OK;
		} else { //"HEAD" method
			return OK;
		}

		//Close Handles
		CloseHandle(HStdOut[1]);
		CloseHandle(HStdOut[0]);

	} //End of Main Code

	//*** Memory allcated by apr_palloc (all of this code) will be released by Apache

	return 500; //I think never reach here

} //X_handler

static void register_hooks(apr_pool_t *p){
	ap_hook_handler(x_handler ,0 ,0 , APR_HOOK_FIRST);
}


AP_DECLARE_MODULE(x_cgi) ={
	STANDARD20_MODULE_STUFF,
	0,					/* create per-directory config structure */
	0,					/* merge per-directory config structures */
	0,					/* create per-server config structure */
	0,					/* merge per-server config structures */
	0,					/* command apr_table_t */
	register_hooks		/* register hooks */
};