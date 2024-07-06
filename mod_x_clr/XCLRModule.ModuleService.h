// *
// * XCLRModule (mod_x_clr) :: Apache module for generate contents with CLR based dll (like CGI)
// * (C)2015 Dark Software / Dark Network Systems
// * http://n.dark-x.net/
// *
#define MOD_X_CLR_CLASS
#include "mod_x_clr.h"
#include "XCLRModule.ModuleService.Stream.h"

// This file containts only CLI Commands.
#pragma managed

#using <mscorlib.dll>

using namespace System;
using namespace System::Collections;
using namespace System::IO;
using namespace System::Reflection;

namespace XCLRModule{

	//For User Module
	public ref class ModuleService sealed : MarshalByRefObject {
	internal:
		request_rec *r;
		apr_bucket_brigade *bb;
		apr_bucket *b;
		apr_bucket_alloc_t *ba;

	internal:
		ModuleService(request_rec *r){
			//Initialize Bridge
			this->r = r;
			this->bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
			this->ba = apr_bucket_alloc_create(r->pool);
			this->POST = gcnew(POSTStream)(r);
			this->Output = gcnew(OutputStream)(bb, b, ba);
			this->Headers = gcnew(Hashtable)();
			this->Environments = gcnew(Hashtable)();
		}

	public:
		POSTStream^ POST;
		OutputStream^ Output;

		Hashtable^ Headers;
		Hashtable^ Environments;

		void OutputString(String^ S){
			array<Byte>^ SBin = System::Text::Encoding::UTF8->GetBytes(S);
			int SLength = SBin->Length;
			pin_ptr<Byte> SData = &SBin[0];

			b = apr_bucket_heap_create( (const char *)SData, SLength, 0, this->ba);
			APR_BRIGADE_INSERT_TAIL(bb, b);
			SData = nullptr;
		}
	};

}