// *
// * XCGI Module (mod_x_cgi) :: Another implementation of 'mod_cgi' for Windows Embedded 7 Standard and later Windows / Windows Server
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *
#include "mod_x_cgi.h"

inline int BuildEnvBlock(request_rec *r, EnvBlockManager *EBM, int FileNameLastSlash, char *ENV_SR){
	char* TEMP;

	EBM->MaxBlockSize = MaxMemorySize;
	EBM->EnvBlock = x_palloc_withcheck(r->pool, EBM->MaxBlockSize);
	{
		const apr_array_header_t *env;
		apr_table_entry_t *e;
		int i;

		ap_add_cgi_vars(r);

		TEMP = (char*) apr_table_get(r->subprocess_env, "PATH_TRANSLATED");
		if(TEMP != 0){
			BackSlashToSlash(TEMP, MaxMemorySize); //100% null contains by apr. so Memory Size is dummy.
			apr_table_set(r->subprocess_env, "PATH_TRANSLATED", TEMP+2);
		}
		
		//apr_table_set(r->subprocess_env, "PATH", ".");
		//apr_table_set(r->subprocess_env, "AUTH_TYPE", ".");
		apr_table_set(r->subprocess_env, "CONTENT_LENGTH", apr_table_get(r->headers_in, "Content-Length"));
		apr_table_set(r->subprocess_env, "CONTENT_TYPE", apr_table_get(r->headers_in, "Content-Type"));
		apr_table_set(r->subprocess_env, "GATEWAY_INTERFACE", "CGI/1.1 XCGI/1.0");
		apr_table_set(r->subprocess_env, "REMOTE_ADDR", r->useragent_ip);
		apr_table_set(r->subprocess_env, "REMOTE_USER", r->user);
		//apr_table_set(r->subprocess_env, "SCRIPT_FILE", (r->filename) + FileNameLastSlash );
		apr_table_set(r->subprocess_env, "SCRIPT_FILENAME", (r->filename)+2);
		apr_table_set(r->subprocess_env, "SERVER_SOFTWARE", "Project CGI Loader");

		if(ENV_SR != 0){
			apr_table_set(r->subprocess_env, "SystemRoot", ENV_SR);
		}


		env = apr_table_elts(r->subprocess_env);
		for (i = 0; i < env->nelts; i++) {
			e = (apr_table_entry_t *) ( env->elts + ( env->elt_size * i ) );
			AddEnvBlock(EBM, e->key, e->val);							
		}

		env = apr_table_elts(r->headers_in);
		for (i = 0; i < env->nelts; i++) {
			e = (apr_table_entry_t *) ( env->elts + ( env->elt_size * i ) );
			ReplaceChar('-', '_', e->key, MaxMemorySize);
			AddEnvBlockHTTP(EBM, e->key, e->val);							
		}
	}

	//Not smart;;
	ap_add_common_vars(r);
	AddEnvBlock(EBM, "SERVER_PORT", apr_table_get(r->subprocess_env, "SERVER_PORT"));
	AddEnvBlock(EBM, "PATH", apr_table_get(r->subprocess_env, "PATH"));

	
	if (r->path_info && r->path_info[0] != '\0') {
		AddEnvBlock(EBM, "PATH_INFO", r->path_info);
	} else {
		TEMP = (char*) apr_table_get(r->subprocess_env, "REQUEST_URI");
		char *c = TEMP;
		for(int i=0; i<MaxMemorySize; i++){
			c++;
			if(*c == '?'){*c = 0x00;}
			if(*c == 0x00){break;}
		}
		AddEnvBlock(EBM, "PATH_INFO", TEMP);
	}

	return 0;
}