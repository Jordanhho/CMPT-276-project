#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <was/common.h>
#include <was/table.h>

#include "make_unique.h"

#include "ClientUtils.h"

using azure::storage::storage_exception;
using azure::storage::cloud_table;
using azure::storage::cloud_table_client;
using azure::storage::edm_type;
using azure::storage::entity_property;
using azure::storage::table_entity;
using azure::storage::table_operation;
using azure::storage::table_request_options;
using azure::storage::table_result;
using azure::storage::table_shared_access_policy;

using std::cin;
using std::cout;
using std::endl;
using std::getline;
using std::make_pair;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

using web::http::http_headers;
using web::http::http_request;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri;

using web::json::value;

using web::http::experimental::listener::http_listener;

using prop_str_vals_t = vector<pair<string,string>>;

constexpr const char* def_url = "http://localhost:34574";

const string auth_table_name {"AuthTable"};
const string auth_table_userid_partition {"Userid"};
const string auth_table_password_prop {"Password"};
const string auth_table_partition_prop {"DataPartition"};
const string auth_table_row_prop {"DataRow"};
const string data_table_name {"DataTable"};


const string read_entity_op {"ReadEntityAdmin"};
const string update_entity_op {"UpdateEntityAdmin"};
const string push_status_op {"PushStatus"};
const string data_addr {"http://localhost:34568"};
const string friend_updates {"Updates"};

/*
  Given an HTTP message with a JSON body, return the JSON
  body as an unordered map of strings to strings.

  Note that all types of JSON values are returned as strings.
  Use C++ conversion utilities to convert to numbers or dates
  as necessary.
 */
unordered_map<string,string> get_json_body(http_request message) {  
  unordered_map<string,string> results {};
  const http_headers& headers {message.headers()};
  auto content_type (headers.find("Content-Type"));
  if (content_type == headers.end() ||
      content_type->second != "application/json")
    return results;

  value json{};
  message.extract_json(true)
    .then([&json](value v) -> bool
          {
            json = v;
            return true;
          })
    .wait();

  if (json.is_object()) {
    for (const auto& v : json.as_object()) {
      if (v.second.is_string()) {
        results[v.first] = v.second.as_string();
      }
      else {
        results[v.first] = v.second.serialize();
      }
    }
  }
  return results;
}


/*
  Utility to create JSON object value from vector of properties
*/
value build_json_object (const vector<pair<string,string>>& properties) {
    value result {value::object ()};
    for (auto& prop : properties) {
      result[prop.first] = value::string(prop.second);
    }
    return result;
}




/*
  Top-level routine for processing all HTTP GET requests.
 */
void handle_get(http_request message) { 
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PushServer GET " << path << endl;
  auto paths = uri::split_path(path);
  // Need at least an operation and userid
  if (paths.size() < 2) {
    message.reply(status_codes::BadRequest);
    return;
  }
  message.reply(status_codes::NotImplemented);
}

/*
  Top-level routine for processing all HTTP POST requests.
 */
void handle_post(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PushServer POST " << path << endl;
  //split path into paths
  auto paths = uri::split_path(path);
  //need at least an operation, usercountry, username, and status
  if (paths.size() < 4){
    message.reply(status_codes::BadRequest);
    return;
  }
  //get json object
  unordered_map<string,string> json_body{get_json_body (message)};
  if(paths[0] != push_status_op) {
    message.reply(status_codes::BadRequest);
  }
  //do push status
  if(paths[0] == push_status_op && json_body.size() == 1){
    //store everything in message into individual strings
    string user_country {paths[1]};
    string user_name {paths[2]};
    string user_status {paths[3]};

    //grab the string of friends in a vector
    string friends_string{};
    for(const auto v : json_body) {
      friends_string = string(v.second);
    }
     //parse the the friend string for the first item in vector array
    friends_list_t update_list = {parse_friends_list(friends_string)};

    //Iterates through each item in json body
    cout << "requesting friends list from datatable" << endl;
    for(int i = 0; i < update_list.size(); i++) {
      cout << "obtaining get " << update_list[i].first << " and " << update_list[i].second << endl;
      string friend_country {update_list[i].first};
      string friend_name {update_list[i].second};
      //Obtain the friends list from data table using handle GET
      pair<status_code, value> initial_result {
        do_request(methods::GET, data_addr + "/" + read_entity_op + "/" + 
          data_table_name + "/" + friend_country + "/" + friend_name)
      };
      cout << initial_result.first << endl;
      if(initial_result.first == status_codes::OK){
        //Gives status code OK if obtained
        cout << "obtained OK" << endl;
        //Updates the initial friends list
        string updates = get_json_object_prop(initial_result.second, friend_updates);
        string updated_status_list {updates};
        //Checks if the obtained new json prop is empty or not
        if(updates == "") {
          //initializses it as first status in parameter
          updated_status_list = paths[3];
        }
        else {
          //string concatenation of next statuses
          updated_status_list = updated_status_list + "\n" + paths[3]; 
        }
        cout << updated_status_list << endl;
        //Rebuilds the updated list into the json body
        value updated_json_object {build_json_object(vector<pair<string,string>> {make_pair(friend_updates, updated_status_list)})};

        //Updates the Update table in datatable using handle PUT
        cout << "modifying and putting " << update_list[i].first << " and " << update_list[i].second << endl;
        pair<status_code, value> updated_result {
          do_request(methods::PUT, data_addr + "/" + update_entity_op + "/" + 
            data_table_name + "/" + friend_country + "/" + friend_name, updated_json_object)
        };
        assert(updated_result.first == status_codes::OK);
        cout << "updated OK" << endl;
      }
      else{
        cout << "Non existant person" << endl;
      }
    }
    //After attempting to update, send status OK
    message.reply(status_codes::OK);
    return;  
  }
}

/*
  Top-level routine for processing all HTTP PUT requests.
 */
void handle_put(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PushServer PUT " << path << endl;
}

/*
  Top-level routine for processing all HTTP DELETE requests.
 */
void handle_delete(http_request message) {
  string path {uri::decode(message.relative_uri().path())}; 
  cout << endl << "**** PushServer DELETE " << path << endl;
}


int main (int argc, char const * argv[]) {
  cout << "PushServer: Opening listener" << endl;
  http_listener listener {def_url};
  //listener.support(methods::GET, &handle_get);
  listener.support(methods::POST, &handle_post);
  //listener.support(methods::PUT, &handle_put);
  //listener.support(methods::DEL, &handle_delete);
  listener.open().wait(); // Wait for listener to complete starting

  cout << "Enter carriage return to stop PushServer." << endl;
  string line;
  getline(std::cin, line);

  // Shut it down
  listener.close().wait();
  cout << "PushServer closed" << endl;
}