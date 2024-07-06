// *
// * XCLRModule (mod_x_clr) :: Apache module for generate contents with CLR based dll (like CGI)
// * (C)2015 Dark Software / Dark Network Systems
// * http://n.dark-x.net/
// *
#define MOD_X_CLR_CLASS
#include "mod_x_clr.h"



#using <mscorlib.dll>

using namespace System;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Runtime::InteropServices;

namespace XCLRModule{

	public ref class OutputStream : Stream {
		internal: 
			apr_bucket_brigade *bb;
			apr_bucket *b;
			apr_bucket_alloc_t *ba;

			OutputStream(){} //Block
		public:

			OutputStream(apr_bucket_brigade* bb, apr_bucket* b, apr_bucket_alloc_t* ba){
				this->bb = bb;
				this->b = b;
				this->ba = ba;
			}

			virtual void Flush() override  { return; }
			virtual void SetLength(long long l) override  { return; }
			virtual long long Seek(long long p, System::IO::SeekOrigin so) override { return 0; }

			virtual int Read(array<Byte>^ buffer, int offset, int count) override {
				return 0;
			}

			virtual void Write(array<Byte>^ buffer, int offset, int count) override  { 
				int buffer_len = buffer->Length;
				pin_ptr<Byte> buffer_raw = &buffer[0];

				this->b = apr_bucket_heap_create( (const char *)buffer_raw, buffer_len, 0, this->ba);
				APR_BRIGADE_INSERT_TAIL(this->bb, this->b);
				buffer_raw = nullptr;
				return; 
			}
			
			property long long Length {
				public: virtual long long get() override { return 0; }
			}
			property long long Position {
				public: virtual long long get() override { return 0; }
				public: virtual void set(long long l) override { return; }
			}

			property bool CanRead {
				public: virtual bool get() override { return false; }
			}
			property bool CanWrite {
				public: virtual bool get() override { return true; }
			}
			property bool CanSeek {
				public: virtual bool get() override { return false; }
			}
	};

	public ref class POSTStream : Stream{
		internal: 
			apr_bucket *b;
			apr_bucket_brigade *bb;
			request_rec *r;

			char *CurrentBlock;
			int CurrentBlockRemain;

			bool IsReadable;
			int POSTLength;

			POSTStream(){} //Block

		public:

			POSTStream(request_rec *r){
				this->r = r;

				this->bb = apr_brigade_create(this->r->pool, this->r->connection->bucket_alloc);
				int R = ap_get_brigade(this->r->input_filters, this->bb, AP_MODE_READBYTES, APR_BLOCK_READ, HUGE_STRING_LEN);

				if (R != APR_SUCCESS) {
					this->IsReadable = false;
					return;
				}

				this->b = APR_BRIGADE_FIRST(this->bb);
				this->CurrentBlock = 0;
				this->IsReadable = true;

				this->POSTLength = atoi( apr_table_get(r->headers_in, "Content-Length") );
			}

			virtual void Flush() override  { return; }
			virtual void SetLength(long long l) override  { return; }
			virtual long long Seek(long long p, System::IO::SeekOrigin so) override { return 0; }

			virtual int Read( [In] [Out] array<Byte>^ buffer, int offset, int count) override {
				int buffer_len = (offset + count - 0);
				pin_ptr<Byte> buffer_raw = &buffer[0];

				if( (buffer->Length) < buffer_len){ //Over
					return 0;
				}



				int ReadSize = 0;

				if(this->CurrentBlockRemain == 0){ //New block
					while(true){
						apr_size_t BlockSize;
						const char *Block;

						if( this->b == APR_BRIGADE_SENTINEL(this->bb) ){ //End
							return 0;
						}
						if (APR_BUCKET_IS_EOS(this->b)) { break; }
						if (APR_BUCKET_IS_FLUSH(this->b)){ 	this->b = APR_BUCKET_NEXT(this->b); continue; }

						apr_bucket_read(this->b, (&Block), &BlockSize, APR_BLOCK_READ);

						if( BlockSize <= buffer_len){ //All
							ReadSize = BlockSize;
							memcpy(buffer_raw, Block, ReadSize);
							this->CurrentBlockRemain = 0;
						} else { //Remain
							ReadSize = buffer_len;
							memcpy(buffer_raw, Block, ReadSize);
							this->CurrentBlockRemain = BlockSize - buffer_len;
							this->CurrentBlock = (char*) Block;
							this->CurrentBlock += ReadSize;
						}		
						break;
					}
					this->b = APR_BUCKET_NEXT(this->b);
				} else { //Remaining Block
						if( (this->CurrentBlockRemain) <= buffer_len){ //All
							ReadSize = this->CurrentBlockRemain;
							memcpy(buffer_raw, this->CurrentBlock, ReadSize);
							this->CurrentBlockRemain = 0;
						} else { //Remain
							ReadSize = buffer_len;
							memcpy(buffer_raw, this->CurrentBlock, ReadSize);
							this->CurrentBlockRemain = this->CurrentBlockRemain - ReadSize;
							this->CurrentBlock += ReadSize;
						}

				}

				return ReadSize;
			}

			virtual void Write(array<Byte>^ buffer, int offset, int count) override  { 
				return; 
			}
			
			property long long Length {
				public: virtual long long get() override { return this->POSTLength; }
			}
			property long long Position {
				public: virtual long long get() override { return 0; }
				public: virtual void set(long long l) override { return; }
			}

			property bool CanRead {
				public: virtual bool get() override { return true; }
			}
			property bool CanWrite {
				public: virtual bool get() override { return false; }
			}
			property bool CanSeek {
				public: virtual bool get() override { return false; }
			}
	};


}