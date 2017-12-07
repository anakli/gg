#include <iostream>
#include <redox.hpp>
#include "redis.hh"

#include <fcntl.h>
#include <sys/types.h>
#include "file_descriptor.hh"
#include "exception.hh"

using namespace std;
using namespace redox;
using namespace storage;


//TODO: optimize by re-using connection instead of disconnecting/connecting every time
//TODO: optimize by multi-threading, similar to original S3 implementation

const static std::string UNSIGNED_PAYLOAD = "UNSIGNED-PAYLOAD";

RedisClient::RedisClient( const AWSCredentials & credentials,
						  const RedisClientConfig & config )
  : credentials_( credentials ), config_( config )
{}

void RedisClient::download_file( const string & object,
                              const roost::path & filename )
{

  Redox rdx;
  if(!rdx.connect(config_.hostaddr, 6379)) return;


  Command<string>& c = rdx.commandSync<string>({"GET", object});
  string reply = c.reply();
  
  FileDescriptor file { CheckSystemCall( "open",
    open( filename.string().c_str(), O_RDWR | O_TRUNC | O_CREAT,
          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) ) };

  file.write( reply, true ); 

  rdx.disconnect(); 
}

void RedisClient::upload_files( const vector<PutRequest> & upload_requests,
                                const function<void( const PutRequest & )> & success_callback )
{

  Redox rdx;
  if(!rdx.connect(config_.hostaddr, 6379)) {
	 cout << "ERROR: could not connect to redis server " << config_.hostaddr << "\n";
	 return;
  }

  cout << "connected to redis server... "; 

  //TODO: multi-thread optimization
  for ( size_t file_id = 0;
	file_id < upload_requests.size();
	file_id++ ) {
    const string & filename = upload_requests.at( file_id ).filename.string();
	const string & object_key = upload_requests.at( file_id ).object_key;
	string hash = upload_requests.at( file_id ).content_hash.get_or( UNSIGNED_PAYLOAD );

	string contents;
	FileDescriptor file { CheckSystemCall( "open " + filename, open( filename.c_str(), O_RDONLY ) ) };
	while ( not file.eof() ) { contents.append( file.read() ); }
	file.close();

	rdx.set(object_key, contents);

    success_callback( upload_requests[ file_id ] ); //not necessary
  }
  rdx.disconnect(); 
}


void RedisClient::download_files( const std::vector<storage::GetRequest> & download_requests,
                                  const std::function<void( const storage::GetRequest & )> & success_callback )
{
  Redox rdx;
  if(!rdx.connect(config_.hostaddr, 6379)) return; 
  

  //TODO: multi-thread optimization
  for ( size_t file_id = 0;
	file_id < download_requests.size();
	file_id ++ ) {
    const string & object_key = download_requests.at( file_id ).object_key;
    
	//string reply = rdx.get(object_key);
	Command<string>& c = rdx.commandSync<string>({"GET", object_key});
    string reply = c.reply();
	
	const roost::path & filename = download_requests.at( file_id ).filename; 
	FileDescriptor file { CheckSystemCall( "open",
										  open( filename.string().c_str(), O_RDWR | O_TRUNC | O_CREAT,
												S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) ) };

	file.write( reply, true ); 
    success_callback( download_requests[ file_id ] ); //not necessary

  }
  rdx.disconnect(); 
}


