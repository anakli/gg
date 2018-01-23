#include <iostream>
#include <istream>
#include <sstream>
#include "crail.hh"

#include <fcntl.h>
#include <sys/types.h>
#include "file_descriptor.hh"
#include "exception.hh"


using namespace std;
using namespace storage;

std::vector<std::string> split(const std::string& s, char delimiter)
{
   std::vector<std::string> tokens;
   std::string token;
   std::istringstream tokenStream(s);
   while (std::getline(tokenStream, token, delimiter))
   {
      tokens.push_back(token);
   }
   return tokens;
}

//TODO: optimize by re-using connection instead of disconnecting/connecting every time
//TODO: optimize by multi-threading, similar to original S3 implementation

const static std::string UNSIGNED_PAYLOAD = "UNSIGNED-PAYLOAD";

CrailClient::CrailClient( const AWSCredentials & credentials,
						  const CrailClientConfig & config )
  : credentials_( credentials ), config_( config )
{
}

void CrailClient::download_file( const string & object,
                              const roost::path & filename )
{
  std::string command = "/home/ubuntu/crail/crail-deployment/crail-1.0/bin/crail fs -copyToLocal " + object + " " + filename.string();
  std::cout << "Execute command: " << command;
  int err = system(command.c_str());
  if (err != 0) {
	    std::cout << "Error from crail command is: " << err;
  }

}
//FIXME: assumes need to upload files all in same deps dir
void CrailClient::upload_files( const vector<PutRequest> & upload_requests,
                                const function<void( const PutRequest & )> & success_callback )
{
  const string & filename = upload_requests.at( 0 ).filename.string();
  vector<string> tokens = split(filename, '/');
  string dir = "";
  for (size_t i = 1; i < tokens.size() -1 ; i++) {
	dir += "/" + tokens[i];
  }
  std::cout << "Dir is: " << dir;
  std::string command = "/home/ubuntu/crail/crail-deployment/crail-1.0/bin/crail fs -copyFromLocal " + dir + "/* /";
  std::cout << "Execute command: " << command;
  int err = system(command.c_str());
  if (err != 0) {
	    std::cout << "Error from crail command is: " << err;
  }

  for ( size_t file_id = 0;
	file_id < upload_requests.size();
	file_id ++ ) {
    
  	success_callback( upload_requests[ file_id ] ); //not necessary
  }
}

//FIXME: assume need to download files all in same dir
void CrailClient::download_files( const std::vector<storage::GetRequest> & download_requests,
                                  const std::function<void( const storage::GetRequest & )> & success_callback )
{
  const string & src_filename = download_requests.at( 0 ).object_key;
  const string & dst_filename = download_requests.at( 0 ).filename.string(); 
  
  vector<string> tokens = split(src_filename, '/');
  string src_dir = "";
  for (size_t i = 1; i < tokens.size() -1 ; i++) {
	src_dir += "/" + tokens[i];
  }
  std::cout << "src dir is: " << src_dir;
  tokens = split(dst_filename, '/');
  string dst_dir = "";
  for (size_t i = 1; i < tokens.size() -1 ; i++) {
	dst_dir += "/" + tokens[i];
  }
  std::cout << "dst dir is: " << dst_dir;
  std::string command = "/home/ubuntu/crail/crail-deployment/crail-1.0/bin/crail fs -copyToLocal " + src_dir + "/* " + dst_dir + "/";
  std::cout << "Execute command: " << command;
  int err = system(command.c_str());
  if (err != 0) {
	    std::cout << "Error from crail command is: " << err;
  }
  for ( size_t file_id = 0;
	file_id < download_requests.size();
	file_id ++ ) {
    
  	success_callback( download_requests[ file_id ] ); //not necessary
  }
}


