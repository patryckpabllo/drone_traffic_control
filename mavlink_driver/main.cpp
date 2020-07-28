#include "client_http.hpp"
#include "server_http.hpp"
#include <future>

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/plugins/info/info.h>

// Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// Added for the default_resource example
#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#ifdef HAVE_OPENSSL
#include "crypto.hpp"
#endif

using namespace std;
// Added for the json-example:
using namespace boost::property_tree;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;


using namespace mavsdk;
using namespace std::this_thread;
using namespace std::chrono;

#define ERROR_CONSOLE_TEXT "\033[31m" // Turn text on console red
#define TELEMETRY_CONSOLE_TEXT "\033[34m" // Turn text on console blue
#define NORMAL_CONSOLE_TEXT "\033[0m" // Restore normal console colour

void usage(std::string bin_name)
{
    std::cout << NORMAL_CONSOLE_TEXT << "Usage : " << bin_name << " <connection_url>" << std::endl
              << "Connection URL format should be :" << std::endl
              << " For TCP : tcp://[server_host][:server_port]" << std::endl
              << " For UDP : udp://[bind_host][:bind_port]" << std::endl
              << " For Serial : serial:///path/to/serial/dev[:baudrate]" << std::endl
              << "For example, to connect to the simulator use URL: udp://:14540" << std::endl;
}

void component_discovered(ComponentType component_type)
{
    std::cout << NORMAL_CONSOLE_TEXT << "Discovered a component with type "
              << unsigned(component_type) << std::endl;
}

void httpClient(const std::string &method, const std::string &path, std::istream &content)
{
     std::cout << "Method:" << method  << std::endl
               << "Path:" << path  << std::endl
               << "Content" << content.rdbuf()  << std::endl;

    HttpClient client("localhost:7090");
    try {

        client.request(method, path, content);
    }
    catch(const SimpleWeb::system_error &e) {
      cerr << "Client request error: " << e.what() << endl;
    }
}

int is_result_error(const Telemetry::Result &result)
{
    if (result != Telemetry::Result::Success) {
        std::cout << ERROR_CONSOLE_TEXT
                  << "Setting rate failed:" << result
                  << NORMAL_CONSOLE_TEXT << std::endl;
        return 1;
    }

    return 0;
}

Mavsdk dc;

int main(int argc, char** argv) {
  // HTTP-server at port 8080 using 1 thread
  // Unless you do more heavy non-threaded processing in the resources,
  // 1 thread is usually faster than several threads
  HttpServer server;
  server.config.port = 8080;

  std::string connection_url;
  ConnectionResult connection_result;

  bool discovered_system = false;
  if (argc == 2) {
      connection_url = argv[1];
      connection_result = dc.add_any_connection(connection_url);
  } else {
      usage(argv[0]);
      return 1;
  }

  if (connection_result != ConnectionResult::Success) {
      std::cout << ERROR_CONSOLE_TEXT
                << "Connection failed: " << connection_result
                << NORMAL_CONSOLE_TEXT << std::endl;
      return 1;
  }

  // We don't need to specify the UUID if it's only one system anyway.
  // If there were multiple, we could specify it with:
  // dc.system(uint64_t uuid);
  System& system = dc.system();

  std::cout << "Waiting to discover system..." << std::endl;
  dc.register_on_discover([&discovered_system](uint64_t uuid) {
      std::cout << "Discovered system with UUID: " << uuid << std::endl;
      discovered_system = true;
  });

  // We usually receive heartbeats at 1Hz, therefore we should find a system after around 2
  // seconds.
  sleep_for(seconds(2));

  if (!discovered_system) {
      std::cout << ERROR_CONSOLE_TEXT << "No system found, exiting." << NORMAL_CONSOLE_TEXT
                << std::endl;
      return 1;
  }

  // Register a callback so we get told when components (camera, gimbal) etc
  // are found.
  system.register_component_discovered_callback(component_discovered);

  auto telemetry = std::make_shared<Telemetry>(system);


{  // We want to listen to the position of the drone at 1 Hz.
  const Telemetry::Result set_rate_result = telemetry->set_rate_position(1.0);
  if (is_result_error(set_rate_result)) {
      return 1;
  }

 // Set up callback to monitor battery while the vehicle is in flight
  telemetry->subscribe_position([](Telemetry::Position position) {
      stringstream json_string;

      //Example: [lat: 47.3964, lon: 8.54649, abs_alt: 538.031, rel_alt: 49.993]
      json_string << "{ "
                  << "\"latitude\" : " << position.latitude_deg << ", "
                  << "\"longitude\" : " << position.longitude_deg << ", "
                  << "\"abs_altitude\" : " << position.absolute_altitude_m << ", "
                  << "\"rel_altitude\" : " << position.relative_altitude_m
                  << " }";

      httpClient("POST", "/position", json_string);
  });
}


{  // We want to listen to the battery of the drone at 1 Hz.
  const Telemetry::Result set_rate_battery = telemetry->set_rate_battery(1.0);
  if (is_result_error(set_rate_battery)) {
      return 1;
  }

  // Set up callback to monitor position while the vehicle is in flight
  telemetry->subscribe_battery([](Telemetry::Battery battery) {
      stringstream json_string;

      //Example: [voltage_v: 12.15, remaining_percent: 1]
      json_string << "{ "
                  << "\"voltage\" : " << battery.voltage_v << ", "
                  << "\"remaining_percent\" : " << battery.remaining_percent
                  << " }";

      httpClient("POST", "/battery", json_string);
  });
}

  {  // We want to listen to the gps info of the drone at 1 Hz.
    const Telemetry::Result set_rate_gps_info = telemetry->set_rate_gps_info(1.0);
    if (is_result_error(set_rate_gps_info)) {
        return 1;
    }

    // Set up callback to monitor position while the vehicle is in flight
    telemetry->subscribe_gps_info([](Telemetry::GpsInfo gpsInfo) {
        stringstream json_string;

        /**< @brief Fix type (0: no GPS, 1: no fix, 2: 2D fix, 3: 3D fix, 4: DGPS fix,
                                             5: RTK float, 6: RTK fixed). */
        std::string fix = "";
        switch(gpsInfo.fix_type) {
            case Telemetry::FixType::NoGps:
                fix = "no GPS";
                break;
            case Telemetry::FixType::NoFix:
                fix = "no fix";
                break;
            case Telemetry::FixType::Fix2D:
                fix = "2D fix";
                break;
            case Telemetry::FixType::Fix3D:
                fix = "3D fix";
                break;
            case Telemetry::FixType::FixDgps:
                fix = "DGPS fix";
                break;
            case Telemetry::FixType::RtkFloat:
                fix = "RTK float";
                break;
            case Telemetry::FixType::RtkFixed:
                fix = "RTK fixed";
                break;
            default:
                fix = "unknown";
        }

        json_string << "{ "
                    << "\"fix_type\" : \"" << fix << "\", "
                    << "\"num_satellites\" : " << gpsInfo.num_satellites
                    << " }";

        httpClient("POST", "/gpsinfo", json_string);
    });
  }


  {  // We want to listen to the gps info of the drone at 1 Hz.
    const Telemetry::Result set_rate_home_position = telemetry->set_rate_home(1.0);
    if (is_result_error(set_rate_home_position)) {
        return 1;
    }

    // Set up callback to monitor position while the vehicle is in flight
    telemetry->subscribe_home([](Telemetry::Position position) {
        stringstream json_string;

        json_string << "{ "
                    << "\"latitude\" : " << position.latitude_deg << ", "
                    << "\"longitude\" : " << position.longitude_deg << ", "
                    << "\"abs_altitude\" : " << position.absolute_altitude_m << ", "
                    << "\"rel_altitude\" : " << position.relative_altitude_m
                    << " }";

        httpClient("POST", "/homepostion", json_string);
    });
  }

  {  // We want to listen to the fixed wing metrics of the drone at 1 Hz.
    const Telemetry::Result set_rate_fixedwing_metrics = telemetry->set_rate_fixedwing_metrics(1.0);
    if (is_result_error(set_rate_fixedwing_metrics)) {
        return 1;
    }

    // Set up callback to monitor position while the vehicle is in flight
    telemetry->subscribe_fixedwing_metrics([](Telemetry::FixedwingMetrics metrics) {
        stringstream json_string;

        json_string << "{ "
                    << "\"airspeed\" : " << metrics.airspeed_m_s << ", "
                    << "\"climb_rate\" : " << metrics.climb_rate_m_s << ", "
                    << "\"throttle\" : " << metrics.throttle_percentage
                    << " }";

        httpClient("POST", "/fixedwingmetrics", json_string);
    });
  }

  {  // We want to listen to the arm status of the drone
    telemetry->subscribe_armed([](bool armed) {
        stringstream json_string;

        json_string << "{ "
                    << "\"armed\" : " << (armed ? "true" : "false")
                    << " }";

        httpClient("POST", "/isarmed", json_string);
    });
  }

  {  // We want to listen to the arm status of the drone
    telemetry->subscribe_flight_mode([](Telemetry::FlightMode flight_mode) {
        stringstream json_string;


        json_string << "{ "
                    << "\"flight_mode\" : \"" << flight_mode << "\""
                    << " }";

        httpClient("POST", "/flightmode", json_string);
    });
  }










  // API service implementation sessions below

  // GET-example for the path /info
  // Responds with request-information
  server.resource["^/systeminfo$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {

      auto content = request->content.string();

      // We don't need to specify the UUID if it's only one system anyway.
      // If there were multiple, we could specify it with:
      // dc.system(uint64_t uuid);
      System& system = dc.system();

      stringstream jsonStream;

      // Get system UUID
      jsonStream << " { \"uuid\": \"" << system.get_uuid() << "\" }" ;
/*

      auto info = std::make_shared<Info>(system);

      // Wait until version/firmware information has been populated from the vehicle
      while (info->get_identification().first==Info::Result::INFORMATION_NOT_RECEIVED_YET) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
      }

      // Print out the vehicle version information.
      jsonStream << "  \"flight_sw_major\":\"" << info->get_version().second.flight_sw_major << "\","
                 << "  \"flight_sw_minor\": \"" << info->get_version().second.flight_sw_minor << "\","
                 << "  \"flight_sw_patch\": \"" << info->get_version().second.flight_sw_patch << "\","
                 << "  \"flight_sw_vendor_major\": \"" << info->get_version().second.flight_sw_vendor_major << "\","
                 << "  \"flight_sw_vendor_minor\": \"" << info->get_version().second.flight_sw_vendor_minor << "\","
                 << "  \"flight_sw_vendor_patch\": \"" << info->get_version().second.flight_sw_vendor_patch << "\","
                 << "  \"flight_sw_git_hash\": \"" << info->get_version().second.flight_sw_git_hash << "\","
                 << "  \"os_sw_major\": \"" << info->get_version().second.os_sw_major << "\","
                 << "  \"os_sw_minor\": \"" << info->get_version().second.os_sw_minor << "\","
                 << "  \"os_sw_patch\": \"" << info->get_version().second.os_sw_patch << "\","
                 << "  \"os_sw_git_hash\": \"" << info->get_version().second.os_sw_git_hash << "\",";

      // Print out the vehicle product information.
      jsonStream<< "  \"vendor_id\": \"" << info->get_product().second.vendor_id << "\","
                << "  \"vendor_name\": \"" << info->get_product().second.vendor_name << "\","
                << "  \"product_id\": \"" << info->get_product().second.product_id << "\","
                << "  \"product_name\": \"" << info->get_product().second.product_id << "\""
                << " } ";
*/

      response->write(jsonStream);
  };




  // GET-example for the path /info
  // Responds with request-information
  server.resource["^/telemetry$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {

      auto content = request->content.string();

      // We don't need to specify the UUID if it's only one system anyway.
      // If there were multiple, we could specify it with:
      // dc.system(uint64_t uuid);
      System& system = dc.system();

      std::cout << "UID: " << system.get_uuid() << std::endl;
      auto telemetry = std::make_shared<Telemetry>(system);

      telemetry->set_rate_position(10.0);
      std::cout << "Position: " << telemetry->position() << std::endl;

      stringstream stream;
      stream << "Position: " << telemetry->position();
      response->write(stream);
  };

  // Add resources using path-regex and method-string, and an anonymous function
  // POST-example for the path /string, responds the posted string
  server.resource["^/string$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    // Retrieve string:
    auto content = request->content.string();
    // request->content.string() is a convenience function for:
    // stringstream ss;
    // ss << request->content.rdbuf();
    // auto content=ss.str();

    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;


    // Alternatively, use one of the convenience functions, for instance:
    // response->write(content);
  };

  // POST-example for the path /json, responds firstName+" "+lastName from the posted json
  // Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
  // Example posted json:
  // {
  //   "firstName": "John",
  //   "lastName": "Smith",
  //   "age": 25
  // }
  server.resource["^/json$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    try {
      ptree pt;
      read_json(request->content, pt);

      auto name = pt.get<string>("firstName") + " " + pt.get<string>("lastName");

      *response << "HTTP/1.1 200 OK\r\n"
                << "Content-Length: " << name.length() << "\r\n\r\n"
                << name;
    }
    catch(const exception &e) {
      *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n"
                << e.what();
    }


    // Alternatively, using a convenience function:
    // try {
    //     ptree pt;
    //     read_json(request->content, pt);

    //     auto name=pt.get<string>("firstName")+" "+pt.get<string>("lastName");
    //     response->write(name);
    // }
    // catch(const exception &e) {
    //     response->write(SimpleWeb::StatusCode::client_error_bad_request, e.what());
    // }
  };

  // GET-example for the path /info
  // Responds with request-information
  server.resource["^/info$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    stringstream stream;
    stream << "<h1>Request from " << request->remote_endpoint().address().to_string() << ":" << request->remote_endpoint().port() << "</h1>";

    stream << request->method << " " << request->path << " HTTP/" << request->http_version;

    stream << "<h2>Query Fields</h2>";
    auto query_fields = request->parse_query_string();
    for(auto &field : query_fields)
      stream << field.first << ": " << field.second << "<br>";

    stream << "<h2>Header Fields</h2>";
    for(auto &field : request->header)
      stream << field.first << ": " << field.second << "<br>";

    response->write(stream);
  };

  // GET-example for the path /match/[number], responds with the matched string in path (number)
  // For instance a request GET /match/123 will receive: 123
  server.resource["^/match/([0-9]+)$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    response->write(request->path_match[1].str());
  };

  // GET-example simulating heavy work in a separate thread
  server.resource["^/work$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    thread work_thread([response] {
      this_thread::sleep_for(chrono::seconds(5));
      response->write("Work done");
    });
    work_thread.detach();
  };

  // Default GET-example. If no other matches, this anonymous function will be called.
  // Will respond with content in the web/-directory, and its subdirectories.
  // Default file: index.html
  // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
  server.default_resource["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    try {
      auto web_root_path = boost::filesystem::canonical("web");
      auto path = boost::filesystem::canonical(web_root_path / request->path);
      // Check if path is within web_root_path
      if(distance(web_root_path.begin(), web_root_path.end()) > distance(path.begin(), path.end()) ||
         !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
        throw invalid_argument("path must be within root path");
      if(boost::filesystem::is_directory(path))
        path /= "index.html";

      SimpleWeb::CaseInsensitiveMultimap header;

      // Uncomment the following line to enable Cache-Control
      // header.emplace("Cache-Control", "max-age=86400");

#ifdef HAVE_OPENSSL
//    Uncomment the following lines to enable ETag
//    {
//      ifstream ifs(path.string(), ifstream::in | ios::binary);
//      if(ifs) {
//        auto hash = SimpleWeb::Crypto::to_hex_string(SimpleWeb::Crypto::md5(ifs));
//        header.emplace("ETag", "\"" + hash + "\"");
//        auto it = request->header.find("If-None-Match");
//        if(it != request->header.end()) {
//          if(!it->second.empty() && it->second.compare(1, hash.size(), hash) == 0) {
//            response->write(SimpleWeb::StatusCode::redirection_not_modified, header);
//            return;
//          }
//        }
//      }
//      else
//        throw invalid_argument("could not read file");
//    }
#endif

      auto ifs = make_shared<ifstream>();
      ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

      if(*ifs) {
        auto length = ifs->tellg();
        ifs->seekg(0, ios::beg);

        header.emplace("Content-Length", to_string(length));
        response->write(header);

        // Trick to define a recursive function within this scope (for example purposes)
        class FileServer {
        public:
          static void read_and_send(const shared_ptr<HttpServer::Response> &response, const shared_ptr<ifstream> &ifs) {
            // Read and send 128 KB at a time
            static vector<char> buffer(131072); // Safe when server is running on one thread
            streamsize read_length;
            if((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0) {
              response->write(&buffer[0], read_length);
              if(read_length == static_cast<streamsize>(buffer.size())) {
                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                  if(!ec)
                    read_and_send(response, ifs);
                  else
                    cerr << "Connection interrupted" << endl;
                });
              }
            }
          }
        };
        FileServer::read_and_send(response, ifs);
      }
      else
        throw invalid_argument("could not read file");
    }
    catch(const exception &e) {
      response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
    }
  };

  server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
    // Handle errors here
    // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
  };

  // Start server and receive assigned port when server is listening for requests
  promise<unsigned short> server_port;
  thread server_thread([&server, &server_port]() {
    // Start server
    server.start([&server_port](unsigned short port) {
      server_port.set_value(port);
    });
  });
  cout << "Server listening on port " << server_port.get_future().get() << endl << endl;

  // Client examples
  //string json_string = "{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";

  // Synchronous request examples
  /*
  {
    HttpClient client("localhost:8080");
    try {
      cout << "Example GET request to http://localhost:8080/match/123" << endl;
      auto r1 = client.request("GET", "/match/123");
      cout << "Response content: " << r1->content.rdbuf() << endl << endl; // Alternatively, use the convenience function r1->content.string()

      cout << "Example POST request to http://localhost:8080/string" << endl;
      auto r2 = client.request("POST", "/string", json_string);
      cout << "Response content: " << r2->content.rdbuf() << endl << endl;
    }
    catch(const SimpleWeb::system_error &e) {
      cerr << "Client request error: " << e.what() << endl;
    }
  }
  */

  // Asynchronous request example
  /*
  {
    HttpClient client("localhost:8080");
    cout << "Example POST request to http://localhost:8080/json" << endl;
    client.request("POST", "/json", json_string, [](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
      if(!ec)
        cout << "Response content: " << response->content.rdbuf() << endl;
    });
    client.io_service->run();
  }
  */
  server_thread.join();
}

