#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <map>

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
using std::map;
using std::tuple;
using std::make_tuple;
using std::get;

using web::http::http_headers;
using web::http::http_request;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri;

using web::json::value;
using web::json::object;

using web::http::experimental::listener::http_listener;

using prop_vals_t = vector<pair<string,value>>;
using prop_str_vals_t = vector<pair<string,string>>;

constexpr const char* def_url = "http://localhost:34572";

const string auth_addr {"http://localhost:34570"};
const string auth_table_name {"AuthTable"};
const string auth_table_userid_partition {"Userid"};
const string auth_table_password_prop {"Password"};
const string auth_table_partition_prop {"DataPartition"};
const string auth_table_row_prop {"DataRow"};
const string token_prop {"token"};
const string friend_prop {"Friends"};
const string status_prop {"Status"};

const string data_addr {"http://localhost:34568"};
const string data_table_name {"DataTable"};

const string push_addr {"http://localhost:34574"};
const string push_status_op {"PushStatus"};

const string sign_on_op {"SignOn"};
const string sign_off_op {"SignOff"};
const string add_friend_op {"AddFriend"};
const string unfriend_op {"UnFriend"};
const string update_status_op {"UpdateStatus"};
const string read_friend_list_op {"ReadFriendList"};

const string read_entity_op {"ReadEntityAuth"};
const string update_entity_op {"UpdateEntityAuth"};

const string get_update_data_op {"GetUpdateData"};

map<string,tuple<string,string,string>> signed_on_users {};

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
  Top-level routine for processing all HTTP GET requests.
 */
void handle_get(http_request message) { 
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** UserServer GET " << path << endl;
  auto paths = uri::split_path(path);
  // Need at least an operation and userid
  if (paths.size() < 2) {
    message.reply(status_codes::BadRequest);
    return;
  }
  //read friend list
  if (paths[0] == read_friend_list_op){
    string user_name {paths[1]};
    string user_token;
    string user_part;
    string user_row;
    bool signed_on {false};
    map<string,tuple<string,string,string>>::iterator it {signed_on_users.find(user_name)};
    if(it != signed_on_users.end()){
      signed_on = true;
      for(auto v : signed_on_users){
        if(v.first == user_name){
          user_token = get<0>(v.second);
          user_part = get<1>(v.second);
          user_row = get<2>(v.second);
        }
      }
    }
    if(signed_on){
      pair<status_code,value> read_result = do_request(methods::GET, data_addr + "/" + read_entity_op + "/" + data_table_name + "/" + user_token + "/" + user_part + "/" + user_row );
      string friend_list = get_json_object_prop(read_result.second, friend_prop);
      cout << friend_list << endl;
      message.reply(status_codes::OK, value::object(vector<pair<string,value>>{make_pair(friend_prop, value::string(friend_list))}));
    }
    else{
      message.reply(status_codes::Forbidden);
    }
  }
}

/*
  Top-level routine for processing all HTTP POST requests.
 */
void handle_post(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** UserServer POST " << path << endl;
  //split path is paths
  auto paths = uri::split_path(path);
  //Need at least an operation and userid
  if (paths.size() < 2){
    message.reply(status_codes::BadRequest);
    return;
  }
  //get json crap
  unordered_map<string,string> json_body {get_json_body (message)};
  string command;

  //signon
  if(paths[0] == sign_on_op && json_body.size() == 1){ //only execute signon if only given password
    string user_name {paths[1]};
    cout << "Username provided is: " << user_name << endl;
    string user_pass;
    for(const auto v : json_body){
      user_pass = string(v.second);
    }
    cout << "password provided is: " << user_pass << endl;
    command = auth_addr + "/" + paths[1];
    pair<status_code,value> token_request_result = do_request(methods::GET,  auth_addr + "/" + get_update_data_op + "/" + user_name, value::object(vector<pair<string,value>>{make_pair(auth_table_password_prop, value::string(user_pass))}));
    if (token_request_result.first == status_codes::OK){//since we're able to get a token we now check data table for such user
      unordered_map <string,string> update_data {unpack_json_object(token_request_result.second)};
      string user_token;
      string user_part;
      string user_row;
      for(const auto v : update_data){
        if(v.first == token_prop){
          user_token = v.second;
        }
        else if(v.first == auth_table_partition_prop){
          user_part = v.second;
        }
        else if(v.first == auth_table_row_prop){
          user_row = v.second;
        }
      }
      cout << "authentication success!! token is: " << user_token << endl;
      pair<status_code,value> read_result = do_request(methods::GET, data_addr + "/" + read_entity_op + "/" + data_table_name + "/" + user_token + "/" + user_part + "/" + user_row);
      if(read_result.first == status_codes::OK){
        bool already_signed_in {false};
        map<string,tuple<string,string,string>>::iterator it {signed_on_users.find(user_name)};
        if(it != signed_on_users.end()){
          cout << "User already signed in" << endl;
          already_signed_in = true;
        }
        if(!already_signed_in){
          signed_on_users.insert(pair<string,tuple<string,string,string>>(user_name, make_tuple(user_token, user_part, user_row)));
        }
        message.reply(status_codes::OK);
        return;
      }
    }
    cout << "SignOn Failed" << endl;
    message.reply(status_codes::NotFound);
    return;
  }

  if(paths[0] == sign_off_op && json_body.size() == 0){
    string user_name {paths[1]};
    map<string,tuple<string,string,string>>::iterator it {signed_on_users.find(user_name)};
    if(it != signed_on_users.end()){
      signed_on_users.erase(it);
      message.reply(status_codes::OK);
      return;
    }
    message.reply(status_codes::NotFound);
    return;
  }
}

/*
  Top-level routine for processing all HTTP PUT requests.
 */
void handle_put(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** UserServer PUT " << path << endl;
  auto paths = uri::split_path(path);

  //add friend
  if(paths[0] == add_friend_op){

    //We need at least an operation, userid, the friend's country and the friend's full name 
    if (paths.size() != 4){
      message.reply(status_codes::BadRequest);
      return;
    }

    //assigning stuff
    string userid {paths[1]};
    string friend_country {paths[2]};
    string friend_name {paths[3]};

    //if the user is signed on
    auto user_check = signed_on_users.find(userid);
    if (user_check != signed_on_users.end()){

      //get users info from the map
      string user_token;
      string user_part;
      string user_row;
      for(const auto v : signed_on_users){
        if(v.first == userid){
          user_token = get<0>(v.second);
          user_part = get<1>(v.second);
          user_row = get<2>(v.second); 
        }
      }

      pair<status_code,value> read_result = do_request(methods::GET, data_addr + "/" + read_entity_op + "/" + data_table_name + "/" + user_token + "/" + user_part + "/" + user_row);
      
      bool is_friend = false;

      //getting friends list
      string friend_list = get_json_object_prop(read_result.second, friend_prop);

      //Turning the friends list string to a friends list object
      friends_list_t friends_list_op = parse_friends_list(friend_list);

      int i =0; 
      for (i=0; i < friends_list_op.size(); i++){

        //Looking to see is there is a matching country and name in the friends list since we don't know if user is adding an existing friends which is really bad as why would you forget who you added; I mean that's a bad friendship right there
        if ((friends_list_op[i].first == friend_country) && friends_list_op[i].second == friend_name){
          is_friend = true;
        }
      }

      //Returns ok if the friend is already in the list
      if (is_friend){
        message.reply(status_codes::OK);
        return;
      }  

      //Appends new friend to the friends list
      friends_list_op.push_back(make_pair(friend_country, friend_name)) ;

      //Turning the friends list object back to a string
      string updated_friend_list = friends_list_to_string(friends_list_op);
      cout << updated_friend_list << endl;

      //puts the updated list back to the user
      value friend_json_object {build_json_object(vector<pair<string,string>> {make_pair(friend_prop, updated_friend_list)})};

      pair<status_code, value> new_result = do_request(methods::PUT, data_addr + "/" + update_entity_op + "/" + data_table_name + "/" + user_token + "/" + user_part + "/" + user_row, friend_json_object);

      assert(new_result.first == status_codes::OK);
      message.reply(status_codes::OK);
      return;
    }
    //if the user isn't signed in
    else{

      message.reply(status_codes::Forbidden);
      return;
    }
  }
  //unfriend :(
  if (paths[0] == unfriend_op){
    if(paths.size() != 4){

      //Need 4 params 
      message.reply(status_codes::BadRequest);
      return;
    }

    //assigning stuff
    string userid {paths[1]};
    string friend_country {paths[2]};
    string friend_name {paths[3]};

    //if the user is signed on
    auto user_check = signed_on_users.find(userid);
    if (user_check != signed_on_users.end()){

      //get users info from the map
      string user_token;
      string user_part;
      string user_row;
      for(const auto v : signed_on_users){
        if(v.first == userid){
          user_token = get<0>(v.second);
          user_part = get<1>(v.second);
          user_row = get<2>(v.second); 
        }
      }

      //gets user data
      pair<status_code,value> read_result = do_request(methods::GET, data_addr + "/" + read_entity_op + "/" + data_table_name + "/" + user_token + "/" + user_part + "/" + user_row);
      bool is_friend = false;

      //getting friends list
      string friend_list = get_json_object_prop(read_result.second, friend_prop);

      //Turning the friends list string to a friends list object
      friends_list_t friends_list_op = parse_friends_list(friend_list);

      int i =0; 
      for (i=0; i < friends_list_op.size(); i++){

        //Looking to see is there is a matching country and name in the friends list and if there's match, the friend is deleted
        if ((friends_list_op[i].first == friend_country) && friends_list_op[i].second == friend_name){
          friends_list_op.erase(friends_list_op.begin()+i);
          is_friend = true;
        }
      }

      //Returns true if the friend wasn't there in the firstplace
      if (is_friend == false){
        message.reply(status_codes::OK);
        return;
      }
      else{
        //Turning the friends list object back to a string
        string updated_friend_list = friends_list_to_string(friends_list_op);

        //puts the updated list back to the user
        value friend_json_object {build_json_object(vector<pair<string,string>> {make_pair(friend_prop, updated_friend_list)})};

        pair<status_code, value> new_result = do_request(methods::PUT, data_addr + "/" + update_entity_op + "/" + data_table_name + "/" + user_token + "/" + user_part + "/" + user_row, friend_json_object);

        //checks if it's there
        assert(new_result.first == status_codes::OK);
        message.reply(status_codes::OK);
        return;
      }
    }
    //if the user isn't signed in
    else{
      
      message.reply(status_codes::Forbidden);
      return;
    }
  }
  //Update Status: has no BadRequest, takes UpdateStatus, uses Userid and status
  if (paths[0] == update_status_op)
  {
    cout << "Updating status" << endl;
    if(paths.size() != 3){

      //Need 3 params : command, userid and status
      message.reply(status_codes::BadRequest);
      return;
    }

    //assigning stuff
    string userid {paths[1]};
    string userstatus {paths[2]};

    cout << "User Status: " << userstatus << endl;

    //if the user is signed on
    auto user_check = signed_on_users.find(userid);
    if (user_check != signed_on_users.end()){

      //get users info from the map
      string user_token;
      string user_part;
      string user_row;
      for(const auto v : signed_on_users){
        if(v.first == userid){
          user_token = get<0>(v.second);
          user_part = get<1>(v.second);
          user_row = get<2>(v.second);

        }
      }
      // get user stuff in order to send to push server 
      string user_name {user_row};
      string user_country {user_part};
      //grabing friend list
      pair<status_code,value> read_result {do_request(methods::GET, data_addr + "/" + read_entity_op + "/" + data_table_name + "/" + user_token + "/" + user_part + "/" + user_row)};
      string friend_list {get_json_object_prop(read_result.second, friend_prop)};

      cout << "User Name: " << user_name << " | User Country: " << user_country << endl;

      // Update the user's status property
      value status_json_object {build_json_object(vector<pair<string,string>> {make_pair(status_prop, userstatus)})};

      pair<status_code, value> status_result = do_request(methods::PUT, data_addr + "/" + update_entity_op +
                                                          "/" + data_table_name + "/" + user_token + "/" +
                                                          "/" + user_part + "/" + user_row,
                                                          status_json_object
                                                          );

      if(status_result.first != status_codes::OK){
        message.reply(status_codes::Forbidden);
        return;
      }
      // put status into everyone else's updates by calling our push server
      string updatestatus = userstatus + "\n";

      //atempts to connect to push server
      try{
        cout << "trying to push now!" << endl;
        cout << "friend list is: " << friend_list << endl;
        cout << "Http request is: " << push_addr + "/" + push_status_op + "/" + user_country + "/" + user_name + "/" + userstatus << endl;
        value friend_json_object {build_json_object(vector<pair<string,string>> {make_pair(friend_prop, friend_list)})};
        pair<status_code, value> push_result = do_request(methods::POST, push_addr + "/" + push_status_op +
                                                          "/" + user_country + "/" + user_name + "/" + userstatus,
                                                          friend_json_object);
        cout << push_result.first << endl;
        message.reply(push_result.first);
        return;
      }
      // if the server isn't running
      catch (const web::uri_exception& e){
        cout << "caught exception!!!" << endl;
        message.reply(status_codes::ServiceUnavailable);
        return;
      }
    }
    else  {
      // Declines not logged in
      message.reply(status_codes::Forbidden);
      return;
    }

  }
  //user typed a wrong command
  else{
    message.reply(status_codes::BadRequest);
    return;
  }
}

/*
  Top-level routine for processing all HTTP DELETE requests.
 */
void handle_delete(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** UserServer DELETE " << path << endl;
}


int main (int argc, char const * argv[]) {
  cout << "UserServer: Opening listener" << endl;
  http_listener listener {def_url};
  listener.support(methods::GET, &handle_get);
  listener.support(methods::POST, &handle_post);
  listener.support(methods::PUT, &handle_put);
  //listener.support(methods::DEL, &handle_delete);
  listener.open().wait(); // Wait for listener to complete starting

  cout << "Enter carriage return to stop UserServer." << endl;
  string line;
  getline(std::cin, line);

  // Shut it down
  listener.close().wait();
  cout << "UserServer closed" << endl;
}