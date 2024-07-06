// *
// * XCLRModule (mod_x_clr) :: Apache module for generate contents with CLR based dll (like CGI)
// * (C)2018-2023 Project Entertainments*
// * http://n.xprj.net/
// *
#include "mod_x_clr.h"

#include <msclr/marshal_cppstd.h>
#using <mscorlib.dll>
#using <System.Windows.Forms.dll>

using namespace msclr::interop;

using namespace System;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Windows::Forms;



static int x_handler(request_rec *r){

	//Handler
	if( strcmp(r->handler, "x-clr-dll") ) {
		return DECLINED;
	}
	//End of Handler

	//Script 404 checker
	if(r->finfo.filetype == APR_NOFILE) {
		//Log(r, APLOG_NOTICE, "Script file not found [%s]", r->filename);
		return HTTP_NOT_FOUND;
	}
	//End of 404 checker

	//Check DLL is clr managed or unmanaged native
	//if(IsDLLManaged(r->filename)){
	//	return DECLINE;
	//}

	//HTTP Error Code set by dll
	int DLLResult = 0;



		//Classes for Script Service
		XCLRModule::ModuleService^ MS = gcnew(XCLRModule::ModuleService)(r);

		//Create AppDomain
		AppDomain ^AP;
		AppDomainSetup^ APS = gcnew(AppDomainSetup);
		APS->ApplicationBase = Path::GetDirectoryName(Assembly::GetExecutingAssembly()->Location);;
		APS->ApplicationName = "XCLRModule";
		AP = AppDomain::CreateDomain("XCLRModule", nullptr, APS);

		//mod_x_clr Locater
		(AppDomain::CurrentDomain)->AssemblyResolve += gcnew ResolveEventHandler(&XCLRModule::ModuleLoader::mod_x_clr_Loader);

		//Build Environments
		{
			char* TEMP;
			String^ TEMPs;
			const apr_array_header_t *env;
			apr_table_entry_t *e;
			ap_add_cgi_vars(r);



			apr_table_set(r->subprocess_env, "PATH", ".");
			//apr_table_set(r->subprocess_env, "AUTH_TYPE", ".");
			apr_table_set(r->subprocess_env, "CONTENT_LENGTH", apr_table_get(r->headers_in, "Content-Length"));
			apr_table_set(r->subprocess_env, "CONTENT_TYPE", apr_table_get(r->headers_in, "Content-Type"));
			apr_table_set(r->subprocess_env, "GATEWAY_INTERFACE", "CGI/1.1 XCGI/1.0 XCLR/1.0");
			apr_table_set(r->subprocess_env, "REMOTE_ADDR", r->useragent_ip);
			apr_table_set(r->subprocess_env, "REMOTE_USER", r->user);
			//apr_table_set(r->subprocess_env, "SCRIPT_FILE", (r->filename) + FileNameLastSlash );
			apr_table_set(r->subprocess_env, "SCRIPT_FILENAME", (r->filename)+2);
			apr_table_set(r->subprocess_env, "SERVER_SOFTWARE", "Project CLR Loader");

			env = apr_table_elts(r->subprocess_env);
			for (int i = 0; i < env->nelts; i++) {
				e = (apr_table_entry_t *) ( env->elts + ( env->elt_size * i ) );
				MS->Environments[ marshal_as<System::String^>(e->key) ] = marshal_as<System::String^>(e->val);
			}

			env = apr_table_elts(r->headers_in);
			for (int i = 0; i < env->nelts; i++) {
				e = (apr_table_entry_t *) ( env->elts + ( env->elt_size * i ) );
				TEMPs = marshal_as<System::String^>(e->key);
				TEMPs = TEMPs->Replace('-', '_')->ToUpper();
				MS->Environments[ "HTTP_"+TEMPs ] = marshal_as<System::String^>(e->val);
			}

			TEMP = (char*) apr_table_get(r->subprocess_env, "PATH_TRANSLATED");
			if(TEMP != 0){
				TEMPs = ( marshal_as<System::String^>(TEMP+2) );
				TEMPs = TEMPs->Replace('\\', '/');
				MS->Environments["PATH_TRANSLATED"] = TEMPs;
			}

			ap_add_common_vars(r);
			MS->Environments["SERVER_PORT"] = marshal_as<System::String^>( apr_table_get(r->subprocess_env, "SERVER_PORT") );

		}
		//Load & Exec Script
		try{
			XCLRModule::ModuleLoader^ ML = (XCLRModule::ModuleLoader^) AP->CreateInstanceAndUnwrap(Assembly::GetExecutingAssembly()->FullName, XCLRModule::ModuleLoader::typeid->FullName);
			ML->Load( marshal_as<System::String^>(r->filename) );
			DLLResult = ML->Main(MS);
		} catch(Exception^ E) {
			MS->OutputString("CLR Module: Exception occured\n");
			MS->OutputString(E->Message + "\n\n");
			MS->OutputString(E->Source + "\n\n");
			MS->OutputString(E->StackTrace + "\n\n");
		}

		//Build Headers
		{
			r->headers_out = apr_table_make(r->pool, 8);
			marshal_context ctx;
			const char* Key;
			const char* Value;

			for each(String^ KeyID in MS->Headers->Keys) {
				Key = ctx.marshal_as<const char*>( ((String^)KeyID) );
				Value = ctx.marshal_as<const char*>( ((String^)MS->Headers[KeyID]) );
				apr_table_add(r->headers_out, Key, Value);
			}
			r->content_type = apr_table_get(r->headers_out, "Content-Type");
		}

		//Free
		APS = nullptr;
		AppDomain::Unload(AP);

		//End of structs
		APR_BRIGADE_INSERT_TAIL(MS->bb, apr_bucket_eos_create(r->connection->bucket_alloc) );

		//Pass to output filters
		ap_pass_brigade(r->output_filters, MS->bb);


	
	return DLLResult;
}


static void register_hooks(apr_pool_t *p){
	ap_hook_handler(x_handler, 0, 0, APR_HOOK_FIRST);
}

AP_DECLARE_MODULE(x_clr) ={
	STANDARD20_MODULE_STUFF,
	0,					/* create per-directory config structure */
	0,					/* merge per-directory config structures */
	0,					/* create per-server config structure */
	0,					/* merge per-server config structures */
	0,					/* command apr_table_t */
	register_hooks		/* register hooks */
};