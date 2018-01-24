/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef CRAIL_HH
#define CRAIL_HH

#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include "aws.hh"
#include "http_request.hh"
#include "path.hh"
#include "optional.hh"
#include "storage_requests.hh"

using namespace std;



struct CrailClientConfig
{
  std::string namenode_addr {};
  int port = 9060;
  //size_t max_threads { 32 };
  //size_t max_batch_size { 32 };
};

class CrailClient
{
private:
  AWSCredentials credentials_;
  CrailClientConfig config_;

public:
  CrailClient( const AWSCredentials & credentials,
			   const CrailClientConfig & config = {} );

  void download_file( const std::string & object,
                      const roost::path & filename );

  void upload_files( const std::vector<storage::PutRequest> & upload_requests,
                     const std::function<void( const storage::PutRequest & )> & success_callback
                       = []( const storage::PutRequest & ){} );

  void download_files( const std::vector<storage::GetRequest> & download_requests,
                       const std::function<void( const storage::GetRequest & )> & success_callback
                         = []( const storage::GetRequest & ){} );
};


#endif /* CRAIL_HH */
