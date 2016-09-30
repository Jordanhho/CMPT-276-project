  /*
  Sample unit tests for BasicServer
 */

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include <pplx/pplxtasks.h>

#include <UnitTest++/UnitTest++.h>

using std::cerr;
using std::cout;
using std::endl;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

using web::http::http_headers;
using web::http::http_request;
using web::http::http_response;
using web::http::method;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri_builder;

using web::http::client::http_client;

using web::json::object;
using web::json::value;

const string create_table_op {"CreateTableAdmin"};
const string delete_table_op {"DeleteTableAdmin"};

const string read_entity_admin {"ReadEntityAdmin"};
const string update_entity_admin {"UpdateEntityAdmin"};
const string delete_entity_admin {"DeleteEntityAdmin"};

const string read_entity_auth {"ReadEntityAuth"};
const string update_entity_auth {"UpdateEntityAuth"};

const string get_read_token_op  {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};

// The two optional operations from Assignment 1
const string add_property_admin {"AddPropertyAdmin"};
const string update_property_admin {"UpdatePropertyAdmin"};

// Our extensions ========================================================================================================================
//additional operations from user and push server
const string sign_on_op {"SignOn"};
const string sign_off_op {"SignOff"};
const string add_friend_op {"AddFriend"};
const string unfriend_op {"UnFriend"};
const string update_status_op {"UpdateStatus"};
const string read_friend_list_op {"ReadFriendList"};
const string push_status_op {"PushStatus"};
// End of our extensions =================================================================================================================


/*
  Make an HTTP request, returning the status code and any JSON value in the body

  method: member of web::http::methods
  uri_string: uri of the request
  req_body: [optional] a json::value to be passed as the message body

  If the response has a body with Content-Type: application/json,
  the second part of the result is the json::value of the body.
  If the response does not have that Content-Type, the second part
  of the result is simply json::value {}.

  You're welcome to read this code but bear in mind: It's the single
  trickiest part of the sample code. You can just call it without
  attending to its internals, if you prefer.
 */

// Version with explicit third argument
pair<status_code,value> do_request (const method& http_method, const string& uri_string, const value& req_body) {
  http_request request {http_method};
  if (req_body != value {}) {
    http_headers& headers (request.headers());
    headers.add("Content-Type", "application/json");
    request.set_body(req_body);
  }

  status_code code;
  value resp_body;
  http_client client {uri_string};
  client.request (request)
    .then([&code](http_response response)
          {
            code = response.status_code();
            const http_headers& headers {response.headers()};
            auto content_type (headers.find("Content-Type"));
            if (content_type == headers.end() ||
                content_type->second != "application/json")
              return pplx::task<value> ([] { return value {};});
            else
              return response.extract_json();
          })
    .then([&resp_body](value v) -> void
          {
            resp_body = v;
            return;
          })
    .wait();
  return make_pair(code, resp_body);
}

// Version that defaults third argument
pair<status_code,value> do_request (const method& http_method, const string& uri_string) {
  return do_request (http_method, uri_string, value {});
}

/*
  Utility to create a table

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
 */
int create_table (const string& addr, const string& table) {
  pair<status_code,value> result {do_request (methods::POST, addr + create_table_op + "/" + table)};
  return result.first;
}

/*
  Utility to compare two JSON objects

  This is an internal routine---you probably want to call compare_json_values().
 */
bool compare_json_objects (const object& expected_o, const object& actual_o) {
  CHECK_EQUAL (expected_o.size (), actual_o.size());
  if (expected_o.size() != actual_o.size())
    return false;

  bool result {true};
  for (auto& exp_prop : expected_o) {
    object::const_iterator act_prop {actual_o.find (exp_prop.first)};
    CHECK (actual_o.end () != act_prop);
    if (actual_o.end () == act_prop)
      result = false;
    else {
      CHECK_EQUAL (exp_prop.second, act_prop->second);
      if (exp_prop.second != act_prop->second)
        result = false;
    }
  }
  return result;
}

/*
  Utility to compare two JSON objects represented as values

  expected: json::value that was expected---must be an object
  actual: json::value that was actually returned---must be an object
*/
bool compare_json_values (const value& expected, const value& actual) {
  assert (expected.is_object());
  assert (actual.is_object());

  object expected_o {expected.as_object()};
  object actual_o {actual.as_object()};
  return compare_json_objects (expected_o, actual_o);
}

/*
  Utility to compre expected JSON array with actual

  exp: vector of objects, sorted by Partition/Row property 
    The routine will throw if exp is not sorted.
  actual: JSON array value of JSON objects
    The routine will throw if actual is not an array or if
    one or more values is not an object.

  Note the deliberate asymmetry of the how the two arguments are handled:

  exp is set up by the test, so we *require* it to be of the correct
  type (vector<object>) and to be sorted and throw if it is not.

  actual is returned by the database and may not be an array, may not
  be values, and may not be sorted by partition/row, so we have
  to check whether it has those characteristics and convert it 
  to a type comparable to exp.
*/
bool compare_json_arrays(const vector<object>& exp, const value& actual) {
  /*
    Check that expected argument really is sorted and
    that every value has Partion and Row properties.
    This is a precondition of this routine, so we throw
    if it is not met.
  */
  auto comp = [] (const object& a, const object& b) -> bool {
    return a.at("Partition").as_string()  <  b.at("Partition").as_string()
           ||
           (a.at("Partition").as_string() == b.at("Partition").as_string() &&
            a.at("Row").as_string()       <  b.at("Row").as_string()); 
  };
  if ( ! std::is_sorted(exp.begin(),
                         exp.end(),
                         comp))
    throw std::exception();

  // Check that actual is an array
  CHECK(actual.is_array());
  if ( ! actual.is_array())
    return false;
  web::json::array act_arr {actual.as_array()};

  // Check that the two arrays have same size
  CHECK_EQUAL(exp.size(), act_arr.size());
  if (exp.size() != act_arr.size())
    return false;

  // Check that all values in actual are objects
  bool all_objs {std::all_of(act_arr.begin(),
                             act_arr.end(),
                             [] (const value& v) { return v.is_object(); })};
  CHECK(all_objs);
  if ( ! all_objs)
    return false;

  // Convert all values in actual to objects
  vector<object> act_o {};
  auto make_object = [] (const value& v) -> object {
    return v.as_object();
  };
  std::transform (act_arr.begin(), act_arr.end(), std::back_inserter(act_o), make_object);

  /* 
     Ensure that the actual argument is sorted.
     Unlike exp, we cannot assume this argument is sorted,
     so we sort it.
   */
  std::sort(act_o.begin(), act_o.end(), comp);

  // Compare the sorted arrays
  bool eq {std::equal(exp.begin(), exp.end(), act_o.begin(), &compare_json_objects)};
  CHECK (eq);
  return eq;
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
  Utility to delete a table

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
 */
int delete_table (const string& addr, const string& table) {
  // SIGH--Note that REST SDK uses "methods::DEL", not "methods::DELETE"
  pair<status_code,value> result {
    do_request (methods::DEL,
                addr + delete_table_op + "/" + table)};
  return result.first;
}

/*
  Utility to put an entity with a single property

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
  prop: Name of the property
  pstring: Value of the property, as a string
 */
int put_entity(const string& addr, const string& table, const string& partition, const string& row, const string& prop, const string& pstring) {
  pair<status_code,value> result {
    do_request (methods::PUT,
                addr + update_entity_admin + "/" + table + "/" + partition + "/" + row,
                value::object (vector<pair<string,value>>
                               {make_pair(prop, value::string(pstring))}))};
  return result.first;
}

/*
  Utility to put an entity with multiple properties

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
  props: vector of string/value pairs representing the properties
 */
int put_entity(const string& addr, const string& table, const string& partition, const string& row,
              const vector<pair<string,value>>& props) {
  pair<status_code,value> result {
    do_request (methods::PUT,
               addr + update_entity_admin + "/" + table + "/" + partition + "/" + row,
               value::object (props))};
  return result.first;
}

// Our extensions ========================================================================================================================
//  DEFINED FOR TESTING EMPTY JSON
int put_entity(const string& addr, const string& table, const string& partition, const string& row){
  pair<status_code,value> result {
    do_request (methods::PUT,
    addr + update_entity_admin + "/" + table + "/" + partition + "/" + row)};
  return result.first;
}

int add_prop(const string& addr, const string& table, const string& prop, const string& pstring) {
  pair<status_code,value> result {
    do_request (methods::PUT,
    addr + add_property_admin + "/" + table,
    value::object (vector<pair<string,value>>
             {make_pair(prop, value::string(pstring))}))};
  return result.first;
}

int add_prop(const string& addr, const string& table) {
  pair<status_code,value> result {
    do_request (methods::PUT,
    addr + add_property_admin + "/" + table)};
  return result.first;
}

int update_prop(const string& addr, const string& table, const string& prop, const string& pstring) {
  pair<status_code,value> result {
    do_request (methods::PUT,
    addr + update_property_admin + "/" + table,
    value::object (vector<pair<string,value>>
             {make_pair(prop, value::string(pstring))}))};
  return result.first;
}

int update_prop(const string& addr, const string& table) {
  pair<status_code,value> result {
    do_request (methods::PUT,
    addr + update_property_admin + "/" + table)};
  return result.first;
}

// End of our extensions =================================================================================================================

/*
  Utility to delete an entity

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
 */
int delete_entity (const string& addr, const string& table, const string& partition, const string& row)  {
  // SIGH--Note that REST SDK uses "methods::DEL", not "methods::DELETE"
  pair<status_code,value> result {
    do_request (methods::DEL,
                addr + delete_entity_admin + "/" + table + "/" + partition + "/" + row)};
  return result.first;
}



/*
  Utility to get a token good for updating a specific entry
  from a specific table for one day.
 */
pair<status_code,string> get_update_token(const string& addr,  const string& userid, const string& password) {
  value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
  pair<status_code,value> result {do_request (methods::GET,
                                              addr +
                                              get_update_token_op + "/" +
                                              userid,
                                              pwd
                                              )};
  cerr << "token " << result.second << endl;
  if (result.first != status_codes::OK)
    return make_pair (result.first, "");
  else {
    string token {result.second["token"].as_string()};
    return make_pair (result.first, token);
  }
}

pair<status_code,string> get_read_token(const string& addr,  const string& userid, const string& password) {
  value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
  pair<status_code,value> result {do_request (methods::GET,
                                              addr +
                                              get_read_token_op + "/" +
                                              userid,
                                              pwd
                                              )};
  cerr << "token " << result.second << endl;
  if (result.first != status_codes::OK)
    return make_pair (result.first, "");
  else {
    string token {result.second["token"].as_string()};
    return make_pair (result.first, token);
  }
}

// Our extensions ========================================================================================================================
//these 2 functions will make and delete users with ease :)

//Utility for making users
int make_user(const string& user_name, const string& user_pass, const string& user_part, const string& user_row) {
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_pwd_prop {"Password"};
  static constexpr const char* auth_part_prop {"DataPartition"};
  static constexpr const char* auth_row_prop {"DataRow"};
  static constexpr const char* friend_prop {"Friends"};
  static constexpr const char* status_prop {"Status"};
  static constexpr const char* update_prop {"Updates"};
  static constexpr const char* null_prop_val {""};

  //put user in the data table
  int put_result {put_entity (addr, table, user_part, user_row, friend_prop, null_prop_val)};
  cerr << "put result " << put_result << endl;
  if (put_result != status_codes::OK) {
    throw std::exception();
  }

  put_result = put_entity(addr, table, user_part, user_row, status_prop, null_prop_val);
  if (put_result != status_codes::OK) {
    throw std::exception();
  }

  put_result = put_entity(addr, table, user_part, user_row, update_prop, null_prop_val);
  if (put_result != status_codes::OK) {
    throw std::exception();
  }

  //put user in the auth table
  put_result = put_entity(addr, auth_table, auth_table_partition, user_name, auth_pwd_prop, user_pass);
  if (put_result != status_codes::OK) {
    throw std::exception();
  }

  put_result = put_entity(addr, auth_table, auth_table_partition, user_name, auth_part_prop, user_part);
  if (put_result != status_codes::OK) {
    throw std::exception();
  }

  put_result = put_entity(addr, auth_table, auth_table_partition, user_name, auth_row_prop, user_row);
  if (put_result != status_codes::OK) {
    throw std::exception();
  }

  return status_codes::OK;
}

int make_ghost(const string& user_name, const string& user_pass, const string& user_part, const string& user_row){
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_pwd_prop {"Password"};
  static constexpr const char* auth_part_prop {"DataPartition"};
  static constexpr const char* auth_row_prop {"DataRow"};

  int put_result {put_entity(addr, auth_table, auth_table_partition, user_name, auth_pwd_prop, user_pass)};
  if (put_result != status_codes::OK) {
    throw std::exception();
  }

  put_result = put_entity(addr, auth_table, auth_table_partition, user_name, auth_part_prop, user_part);
  if (put_result != status_codes::OK) {
    throw std::exception();
  }

  put_result = put_entity(addr, auth_table, auth_table_partition, user_name, auth_row_prop, user_row);
  if (put_result != status_codes::OK) {
    throw std::exception();
  }

  return status_codes::OK;	
}

int delete_user(const string& user_name, const string& user_part, const string& user_row){
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* auth_table_partition {"Userid"};

  int del_ent_result {delete_entity (addr, table, user_part, user_row)};
  if (del_ent_result != status_codes::OK) {
    throw std::exception();
  }

  del_ent_result = delete_entity(addr, auth_table, auth_table_partition, user_name);
  if (del_ent_result != status_codes::OK) {
    throw std::exception();
  }

  return status_codes::OK;
}

int delete_ghost(const string& user_name){
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* auth_table_partition {"Userid"};

  int del_ent_result {delete_entity(addr, auth_table, auth_table_partition, user_name)};
  if (del_ent_result != status_codes::OK) {
    throw std::exception();
  }

  return status_codes::OK;
}

// End of our extensions =================================================================================================================

/*
  A sample fixture that ensures TestTable exists, and
  at least has the entity Franklin,Aretha/USA
  with the property "Song": "RESPECT".

  The entity is deleted when the fixture shuts down
  but the table is left. See the comments in the code
  for the reason for this design.
 */
class BasicFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* table {"TestTable"};
  static constexpr const char* partition {"USA"};
  static constexpr const char* row {"Franklin,Aretha"};
  static constexpr const char* property {"Song"};
  static constexpr const char* prop_val {"RESPECT"};

public:
  BasicFixture() {
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }
    int put_result {put_entity (addr, table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }
  }

  ~BasicFixture() {
    int del_ent_result {delete_entity (addr, table, partition, row)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }

    /*
      In traditional unit testing, we might delete the table after every test.

      However, in cloud NoSQL environments (Azure Tables, Amazon DynamoDB)
      creating and deleting tables are rate-limited operations. So we
      leave the table after each test but delete all its entities.
    */
    cout << "Skipping table delete" << endl;
    /*
      int del_result {delete_table(addr, table)};
      cerr << "delete result " << del_result << endl;
      if (del_result != status_codes::OK) {
        throw std::exception();
      }
    */
  }
};

SUITE(GET) {
  /*
    A test of GET all table entries

    Demonstrates use of new compare_json_arrays() function.
   */
  TEST_FIXTURE(BasicFixture, GetAll) {
    string partition {"Canada"};
    string row {"Katherines,The"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
                  string(BasicFixture::addr)
                  + read_entity_admin + "/"
                  + string(BasicFixture::table))};
    CHECK_EQUAL(status_codes::OK, result.first);
    value obj1 {
      value::object(vector<pair<string,value>> {
          make_pair(string("Partition"), value::string(partition)),
          make_pair(string("Row"), value::string(row)),
          make_pair(property, value::string(prop_val))
      })
    };
    value obj2 {
      value::object(vector<pair<string,value>> {
          make_pair(string("Partition"), value::string(BasicFixture::partition)),
          make_pair(string("Row"), value::string(BasicFixture::row)),
          make_pair(string(BasicFixture::property), value::string(BasicFixture::prop_val))
      })
    };
    vector<object> exp {
      obj1.as_object(),
      obj2.as_object()
    };
    compare_json_arrays(exp, result.second);
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
  }
  // Our extensions =================================================================================================================
    //Case: Test requesting a bad partition
    TEST_FIXTURE(BasicFixture, NoSuchPart) {
      
      string partition {"aProp"};
      pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/"
      + string(BasicFixture::table) + "/"
      + partition)};

      // Test cases in CHECKs
      CHECK_EQUAL(status_codes::BadRequest, result.first);
    }

    // Case: Test 404 Not Found
    TEST_FIXTURE(BasicFixture, NoSuchTable) {
      
      string table {"ouijaboard"};
      pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/"
      + table )};

      // Test cases in CHECKs
      CHECK_EQUAL(status_codes::NotFound, result.first);

    } 
    // Case: Test Missing * in row search name;
  
    TEST_FIXTURE(BasicFixture, MissingRow) {
      string partition {"whyohwhy"};
      string row {"OnDeathRow"};
      string property {"meh"};
      string prop_val{"blah"};
      int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val)};
      cerr << "put result " << put_result << endl;

      pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/"
      + string(BasicFixture::table) + "/"
      + partition )};

      // Test cases in CHECKs
      CHECK_EQUAL(status_codes::BadRequest, result.first);
      CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    }

    //Case: Test Missing table name
    TEST_FIXTURE(BasicFixture, MissingTable) {
      string partition {"whyohwhy"};
      string row {"OnDeathRow"};
      string property {"meh"};
      string prop_val{"blah"};
      int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val)};
      cerr << "put result " << put_result << endl;

      pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/")};

      // Test cases in CHECKs
      CHECK_EQUAL(status_codes::BadRequest, result.first);
      CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    } 

  /*
    A test of GET all entities in a specific partition
    Cases: Get specific partition with 3 partitions
   */
    TEST_FIXTURE(BasicFixture, GetInPart) {
      string partition{"Trump,Donald"};
      string row1 {"Campaign"};
      string property {"Party"};
      string prop_val {"Republican"};
      int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row1, property, prop_val)};
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      string row2 {"Business"};
      property = "Water";
      prop_val =  "TrumpWater";
      put_result =  put_entity (BasicFixture::addr, BasicFixture::table, partition, row2, property, prop_val);
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      property = "Steak";
      prop_val = "TrumpSteaks";
      put_result =  put_entity (BasicFixture::addr, BasicFixture::table, partition, row2, property, prop_val);
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      // Introduce some entities that we don't want to get back from server
      string badpartition{"BADPARTITION"};
      string badrow{"BADROW"};
      string badproperty{"Notgoodbro"};
      string bprop_val("Sumtinwong");

      put_result = put_entity (BasicFixture::addr, BasicFixture::table, badpartition, badrow, badproperty, bprop_val);
      // cerr << "put result " << put_result << endl;
      
      pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/"
      + string(BasicFixture::table) + "/"
      + partition + "/"
      + "*")};

      assert (put_result == status_codes::OK);

      // Test cases in CHECKs
      CHECK_EQUAL(status_codes::OK, result.first);
      CHECK(result.second.is_array());
      CHECK_EQUAL(2, result.second.as_array().size());
      
      CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row1));
      CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row2));
      CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, badpartition, badrow));
    }

    // Case: Test Put no JSON Body;
    TEST_FIXTURE(BasicFixture, NoBodyRequest) {
      string partition {"CantStump"};
      string row {"TheTrump"};
      int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row) };
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/"
      + string(BasicFixture::table) + "/"
      + partition + "/"
      + row + "/"
      + "*" )};

      // Test cases in CHECKs
      CHECK_EQUAL(status_codes::BadRequest, result.first);
      CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
      
    }

  /*
    A test of GET all entities containing specified properties
   */

    TEST_FIXTURE(BasicFixture, GetProperties) { //to create json object use value::object(vector<pair<string,value>>{make_pair("string", value::string("string"))}
      string partition{"Trump,Donald"};
      string row {"Campaign"};
      string property {"Party"};
      string prop_val {"Republican"};
      int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val)};
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      property = "Home";
      prop_val = "Detroit";
      put_result = put_entity (BasicFixture::addr, BasicFixture::table, BasicFixture::partition, BasicFixture::row, property, prop_val);
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      prop_val = "NewYork";
      put_result = put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val);
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      string another_partition{"Sanders,Bernie"};
      string another_row{"Campaign"};
      string another_property {"Party"};
      string another_prop_val {"Democratic"};
      put_result =  put_entity (BasicFixture::addr, BasicFixture::table, another_partition, another_row, another_property, another_prop_val);
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      another_property = "Home";
      another_prop_val = "Burlington";
      put_result =  put_entity (BasicFixture::addr, BasicFixture::table, another_partition, another_row, another_property, another_prop_val);
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      string third_partition {"Trudeau,Justin"};
      string third_row {"Canada"};
      string third_property {"Party"};
      string third_prop_val {"Liberal"};
      put_result = put_entity (BasicFixture::addr, BasicFixture::table, third_partition, third_row, third_property, third_prop_val);
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      string bad_partition {"Trudeau,Pierre"};
      string bad_row {"Canada"};
      string bad_property{"Born"};
      string bad_prop_val{"1919"};
      put_result = put_entity (BasicFixture::addr, BasicFixture::table, bad_partition, bad_row, bad_property, bad_prop_val);
      assert (put_result == status_codes::OK);

      pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/" 
      + string(BasicFixture::table)
      , value::object(vector<pair<string,value>>{make_pair("Party", value::string("*")) , make_pair("Home", value::string("*"))}))};

      CHECK_EQUAL(status_codes::OK, result.first);
      CHECK(result.second.is_array());
      CHECK_EQUAL(2, result.second.as_array().size());
      CHECK_EQUAL(status_codes::OK, delete_entity(BasicFixture::addr, BasicFixture::table, partition, row));
      CHECK_EQUAL(status_codes::OK, delete_entity(BasicFixture::addr, BasicFixture::table, another_partition, another_row));
      CHECK_EQUAL(status_codes::OK, delete_entity(BasicFixture::addr, BasicFixture::table, third_partition, third_row));
      CHECK_EQUAL(status_codes::OK, delete_entity(BasicFixture::addr, BasicFixture::table, bad_partition, bad_row));
    }

    /*
    A test battery on new PUT method
   */

    TEST_FIXTURE(BasicFixture, AddPropNotOK) {
    	
    	//Case: No JSON Body given to server
      CHECK_EQUAL(status_codes::BadRequest, add_prop(BasicFixture::addr, BasicFixture::table));

    	//Case: Call PUT on arbitrary table NOT "/AddProperty/"
    	CHECK_EQUAL(status_codes::NotFound, add_prop(BasicFixture::addr, "Notmytable", "Pies", "Apple"));
    }

    
    TEST_FIXTURE(BasicFixture, AddProperties) {
   	  string partition{"Trump,Donald"};
      string row {"Campaign"};
      string property {"Party"};
      string prop_val {"Republican"};
      int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val)};
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      string second_partition {"Rogan,Seth"};
      string second_row {"USA"};
      string second_property {"Citizenship"};
      string second_prop_val {"Canadian"};
      put_result = put_entity (BasicFixture::addr, BasicFixture::table, second_partition, second_row, second_property, second_prop_val);
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      string add_property {"Citizenship"};
      string add_prop_val {"American"};

      int add_result {add_prop (BasicFixture::addr, BasicFixture::table, add_property, add_prop_val)};
      assert (add_result == status_codes::OK);

	  string third_partition {"Trudeau,Justin"};
      string third_row {"Canada"};
      string third_property {"Citizenship"};
      string third_prop_val {"Canadian"};
      put_result = put_entity (BasicFixture::addr, BasicFixture::table, third_partition, third_row, third_property, third_prop_val);
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/" 
      + string(BasicFixture::table)
      , value::object(vector<pair<string,value>>{make_pair("Citizenship", value::string("American"))}))};

      CHECK_EQUAL(status_codes::OK, result.first);
      CHECK(result.second.is_array());
      CHECK_EQUAL(3, result.second.as_array().size());
      CHECK_EQUAL(status_codes::OK, delete_entity(BasicFixture::addr, BasicFixture::table, partition, row));
      CHECK_EQUAL(status_codes::OK, delete_entity(BasicFixture::addr, BasicFixture::table, second_partition, second_row));
      CHECK_EQUAL(status_codes::OK, delete_entity(BasicFixture::addr, BasicFixture::table, third_partition, third_row));
    }

    TEST_FIXTURE(BasicFixture, UpdatePropNotOK) {
    	
    	//Case: No JSON Body given to server
      CHECK_EQUAL(status_codes::BadRequest, update_prop(BasicFixture::addr, BasicFixture::table));

    	//Case: Call PUT on arbitrary table NOT "/AddProperty/"
    	CHECK_EQUAL(status_codes::NotFound, update_prop(BasicFixture::addr, "Notmytable", "Pies", "Apple"));
    }

    TEST_FIXTURE(BasicFixture, UpdateProperties) {
      string partition{"Trump,Donald"};
      string row {"Campaign"};
      string property {"Party"};
      string prop_val {"Republican"};
      int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val)};
      cerr << "put result " << put_result << endl;
      assert (put_result == status_codes::OK);

      string up_property {"Song"};
      string up_prop_val {"Angel"};
      int up_result {update_prop (BasicFixture::addr, BasicFixture::table, up_property, up_prop_val)};
      assert (up_result == status_codes::OK);

	  pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/" 
      + string(BasicFixture::table)
      , value::object(vector<pair<string,value>>{make_pair("Song", value::string("Angel"))}))};

      CHECK_EQUAL(status_codes::OK, result.first);
      CHECK(result.second.is_array());
      CHECK_EQUAL(1, result.second.as_array().size());
      CHECK_EQUAL(status_codes::OK, delete_entity(BasicFixture::addr, BasicFixture::table, partition, row));
    }

  // End of our extensions =================================================================================================================
}

class AuthFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* auth_addr {"http://localhost:34570/"};
  static constexpr const char* userid {"user"};
  static constexpr const char* user_pwd {"user"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_pwd_prop {"Password"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* partition {"USA"};
  static constexpr const char* row {"Franklin,Aretha"};
  static constexpr const char* property {"Song"};
  static constexpr const char* prop_val {"RESPECT"};

  // Our extensions =================================================================================================================

  //stuff to ensure user has partition and row props
  static constexpr const char* auth_part_prop {"DataPartition"};
  static constexpr const char* auth_row_prop {"DataRow"};

  //stuff for bob
  static constexpr const char* user_bob {"Bob"};
  static constexpr const char* bob_pass {"123abc"};
  static constexpr const char* bob_part {"Pies"};
  static constexpr const char* bob_row {"Apple"};
  // End of our extensions =================================================================================================================


public:
  AuthFixture() {
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }

    
    // Our extensions =================================================================================================================
    //ensuring authtable
    make_result = create_table(addr, auth_table);
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }
    // End of our extensions =================================================================================================================

    int put_result {put_entity (addr, table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }
    // Ensure userid password , row, partition in system
    int user_result {put_entity (addr,
                                 auth_table,
                                 auth_table_partition,
                                 userid,
                                 auth_pwd_prop,
                                 user_pwd)};
    cerr << "user auth table insertion result " << user_result << endl;
    if (user_result != status_codes::OK)
      throw std::exception();

  	// Our extensions =================================================================================================================
  	//don't know why it does not ensure userid has a partition and row???
    user_result = {put_entity(addr,
    						auth_table,
    						auth_table_partition,
    						userid,
    						auth_row_prop,
    						row)};
    cerr << "user auth table insertion result " << user_result << endl;
    if (user_result != status_codes::OK)
      throw std::exception();

    user_result = {put_entity(addr,
    						auth_table,
    						auth_table_partition,
    						userid,
    						auth_part_prop,
    						partition)};
    cerr << "user auth table insertion result " << user_result << endl;
    if (user_result != status_codes::OK)
      throw std::exception();

    //new user bob that is in charge of apple pies!
    int user_result2 {put_entity(addr,
                    auth_table,
                    auth_table_partition,
                    user_bob,
                    auth_pwd_prop,
                    bob_pass)};
    cerr << "user auth table insertion result " << user_result2 << endl;
    if (user_result2 != status_codes::OK)
      throw std::exception();

    user_result2 = {put_entity(addr,
                auth_table,
                auth_table_partition,
                user_bob,
                auth_part_prop,
                bob_part)};
    cerr << "user auth table insertion result " << user_result2 << endl;
    if (user_result2 != status_codes::OK)
      throw std::exception();

    user_result2 = {put_entity(addr,
                auth_table,
                auth_table_partition,
                user_bob,
                auth_row_prop,
                bob_row)};
    cerr << "user auth table insertion result " << user_result2 << endl;
    if (user_result2 != status_codes::OK)
      throw std::exception();
	}
	// End of our extensions =================================================================================================================

  ~AuthFixture() {
    int del_ent_result {delete_entity (addr, table, partition, row)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }

    // Our Extensions =================================================================================================================
    //these just delete users from the auth table (keep things clean :D)
    del_ent_result = delete_entity (addr, auth_table, auth_table_partition, userid);
    if (del_ent_result != status_codes::OK){
    	throw std::exception();
    }
        del_ent_result = delete_entity (addr, auth_table, auth_table_partition, user_bob);
    if (del_ent_result != status_codes::OK){
    	throw std::exception();
    }
    // End of our extensions =================================================================================================================
  }
};

SUITE(UPDATE_AUTH) {

  TEST_FIXTURE(AuthFixture,  PutAuthOK) {

    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};

    //test update with correct update token
    cout << "Requesting update token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::OK, result.first);

    //verify that update was good
    pair<status_code,value> ret_res {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_admin + "/"
                  + AuthFixture::table + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row)};
    CHECK_EQUAL (status_codes::OK, ret_res.first);
    value expect {
      build_json_object (
                         vector<pair<string,string>> {
                           added_prop,
                           make_pair(string(AuthFixture::property), 
                                     string(AuthFixture::prop_val))}
                         )};
                             
    cout << AuthFixture::property << endl;
    compare_json_values (expect, ret_res.second);
  }

  // Our Extensions =================================================================================================================
  TEST_FIXTURE(AuthFixture, PutAuthForbidden){
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};

    //test update with read token
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res {
      get_read_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::Forbidden, result.first);
  }


  //Test Fixture - PutAuth Not Found 
  TEST_FIXTURE(AuthFixture, PutAuthNotFound){
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};

    //test update with bad table
    string trump_table {"TrumpTable"};
    
    cout << "Requesting update token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
    
    pair<status_code,value>result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + trump_table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::NotFound, result.first);
  }

  TEST_FIXTURE(AuthFixture, PutAuthBadRequest){
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};
    cout << "Requesting update token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);

    //Test update without row
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    //Test update without partition and row
    result =
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  );
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    //Test update without everything but table
    result =
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  );
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    //Test update without everything
    result =
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  );
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }
}


SUITE(GET_AUTH){
  TEST_FIXTURE(AuthFixture, GetAuthOK){
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res {
        get_read_token(AuthFixture::auth_addr,
                         AuthFixture::userid,
                           AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row)};
      CHECK_EQUAL (status_codes::OK, result.first);

    value expect {
      build_json_object (
                         vector<pair<string,string>> {
                           make_pair(string(AuthFixture::property), 
                                     string(AuthFixture::prop_val))}
                         )};

    cout << AuthFixture::property << endl;
    compare_json_values (expect, result.second);
  }

  TEST_FIXTURE(AuthFixture, GetAuthBadRequest)
  {
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res {
        get_read_token(AuthFixture::auth_addr,
                            AuthFixture::userid,
                               AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);

    // no row
    pair<status_code,value> result1 {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition
                  )};
    CHECK_EQUAL(status_codes::BadRequest, result1.first);

    // give nothing
    pair<status_code,value> result2 {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_auth
                  )};
    CHECK_EQUAL(status_codes::BadRequest, result2.first);  

    // only table
    pair<status_code,value> result3 {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_auth + "/"
                  + AuthFixture::table
                  )};
    CHECK_EQUAL(status_codes::BadRequest, result3.first);

    //no partition and row
    pair<status_code,value> result4 {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second
                  )};
    CHECK_EQUAL(status_codes::BadRequest, result3.first);
  }

  TEST_FIXTURE(AuthFixture, GetAuthNotFound){
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res {
        get_read_token(AuthFixture::auth_addr,
                            AuthFixture::user_bob,
                               AuthFixture::bob_pass)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);

    //get from non existant table
    string trump_table {"TrumpTable"};
    pair<status_code,value> result1 {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_auth + "/"
                  + trump_table + "/"
                  + token_res.second + "/"
                  + AuthFixture::bob_part + "/"
                  + AuthFixture::bob_row
                  )};
    CHECK_EQUAL(status_codes::NotFound, result1.first);

    //get from non existant entity
    pair<status_code,value> result2 {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::bob_part + "/"
                  + AuthFixture::bob_row
                  )};
    CHECK_EQUAL(status_codes::NotFound, result2.first);

    //get with token that is not authorized for entity
    pair<status_code,value> result3 {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row
                  )};
    CHECK_EQUAL(status_codes::NotFound, result3.first);
  }
}

SUITE(TOKEN_OPS)
{
  TEST_FIXTURE(AuthFixture, ReadTokensOK)
  {
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res {
      get_read_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
  }

  TEST_FIXTURE(AuthFixture, ReadTokensBadRequest)
  {
    //empty password
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res1 {
        get_read_token(AuthFixture::auth_addr,
                            AuthFixture::userid,
                               "")};
    cout << "Token response " << token_res1.first << endl;
    CHECK_EQUAL (token_res1.first, status_codes::BadRequest);

    //empty username
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res2 {
        get_read_token(AuthFixture::auth_addr,
                            "",
                            AuthFixture::user_pwd)};
    cout << "Token response " << token_res2.first << endl;
    CHECK_EQUAL (token_res2.first, status_codes::BadRequest);

    //bad ascii
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res3 {
        get_read_token(AuthFixture::auth_addr,
                            AuthFixture::userid,
                               "¡¤§°±")};
    cout << "Token response " << token_res3.first << endl;
    CHECK_EQUAL (token_res3.first, status_codes::BadRequest);
  }

  TEST_FIXTURE(AuthFixture, ReadTokensNotFound){
    //no such user
    string DonaldTrump {"DonaldTrump"};
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res1 {
        get_read_token(AuthFixture::auth_addr,
                            DonaldTrump,
                            AuthFixture::user_pwd)};
    cout << "Token response " << token_res1.first << endl;
    CHECK_EQUAL (token_res1.first, status_codes::NotFound);

    //wrong password
    cout << "Requesting read token" << endl;
    pair<status_code,string> token_res2 {
        get_read_token(AuthFixture::auth_addr,
                            AuthFixture::userid,
                            AuthFixture::bob_pass)};
    cout << "Token response " << token_res2.first << endl;
    CHECK_EQUAL (token_res2.first, status_codes::NotFound);
  }

  TEST_FIXTURE(AuthFixture, UpdateTokensOK)
  {
    cout << "Requesting update token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
  }

  TEST_FIXTURE(AuthFixture, UpdateTokensBadRequest)
  {
    //empty password
    cout << "Requesting update token" << endl;
    pair<status_code,string> token_res1 {
        get_update_token(AuthFixture::auth_addr,
                            AuthFixture::userid,
                               "")};
    cout << "Token response " << token_res1.first << endl;
    CHECK_EQUAL (token_res1.first, status_codes::BadRequest);

    //empty username
    cout << "Requesting update token" << endl;
    pair<status_code,string> token_res2 {
        get_update_token(AuthFixture::auth_addr,
                            "",
                            AuthFixture::user_pwd)};
    cout << "Token response " << token_res2.first << endl;
    CHECK_EQUAL (token_res2.first, status_codes::BadRequest);

    //bad ascii
    cout << "Requesting update token" << endl;
    pair<status_code,string> token_res3 {
        get_update_token(AuthFixture::auth_addr,
                            AuthFixture::userid,
                               "¡¤§°±")};
    cout << "Token response " << token_res3.first << endl;
    CHECK_EQUAL (token_res3.first, status_codes::BadRequest);
  }

  TEST_FIXTURE(AuthFixture, UpdateTokensNotFound){
    //no such user
    string DonaldTrump {"DonaldTrump"};
    cout << "Requesting update token" << endl;
    pair<status_code,string> token_res1 {
        get_update_token(AuthFixture::auth_addr,
                            DonaldTrump,
                            AuthFixture::user_pwd)};
    cout << "Token response " << token_res1.first << endl;
    CHECK_EQUAL (token_res1.first, status_codes::NotFound);

    //wrong password
    cout << "Requesting update token" << endl;
    pair<status_code,string> token_res2 {
        get_update_token(AuthFixture::auth_addr,
                            AuthFixture::userid,
                            AuthFixture::bob_pass)};
    cout << "Token response " << token_res2.first << endl;
    CHECK_EQUAL (token_res2.first, status_codes::NotFound);
  }
}


class UserFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* auth_addr {"http://localhost:34570/"};
  static constexpr const char* userserver_addr {"http://localhost:34572/"};
  static constexpr const char* push_addr {"http://localhost:34574/"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_pwd_prop {"Password"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* auth_part_prop {"DataPartition"};
  static constexpr const char* auth_row_prop {"DataRow"};
  static constexpr const char* friend_prop {"Friends"};
  static constexpr const char* status_prop {"Status"};
  static constexpr const char* update_prop {"Updates"};
  static constexpr const char* null_prop_val {""};

  //stuff for bob
  static constexpr const char* bob_user {"bob"};
  static constexpr const char* bob_pass {"passw0rd"};
  static constexpr const char* bob_part {"Zimbobwe"};
  static constexpr const char* bob_row {"Mchoy,Bob"};

  //stuff for Baker sensei
  static constexpr const char* baker_user {"ellen"};
  static constexpr const char* baker_pass {"redsox"};
  static constexpr const char* baker_part {"USA"};
  static constexpr const char* baker_row {"Baker,Ellen"};

  //stuff for Donald Trump
  static constexpr const char* trump_user {"trump"};
  static constexpr const char* trump_pass {"MakerAmericaGreatAgain"};
  static constexpr const char* trump_part {"USA"};
  static constexpr const char* trump_row {"Trump,Donald"};

  //stuff for Ted Cruz (not ted from class)
  static constexpr const char* ted_user {"ted"};
  static constexpr const char* ted_pass {"ILuvCanada"};
  static constexpr const char* ted_part {"Canada"};
  static constexpr const char* ted_row {"Cruz,Ted"};

  //stuff for Kinoshita Yuka (youtube her name)
  static constexpr const char* kino_user {"kino"};
  static constexpr const char* kino_pass {"food"};
  static constexpr const char* kino_part {"Japan"};
  static constexpr const char* kino_row {"Yuka,Kinoshita"};

  //stuff for Hillary Clinton
  static constexpr const char* clinton_user {"prezclinton"};
  static constexpr const char* clinton_pass {"prez4lyfe"};
  static constexpr const char* clinton_part {"USA"};
  static constexpr const char* clinton_row {"Clinton,Hillary"};

  //stuff for the ghost
  static constexpr const char* phan_user {"phantom"};
  static constexpr const char* phan_pass {"boo"};
  static constexpr const char* phan_part {"Moon"};
  static constexpr const char* phan_row {"Boo,Phantom"};

public:
  UserFixture() {
    //ensuring datatable
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }

    //ensuring authtable
    make_result = create_table(addr, auth_table);
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }

    int create_user_result {make_user(bob_user, bob_pass, bob_part, bob_row)};
    if (create_user_result != status_codes::OK){
      throw std::exception();
    }

    create_user_result = make_user(baker_user, baker_pass, baker_part, baker_row);
    if (create_user_result != status_codes::OK){
      throw std::exception();
    }

    create_user_result = make_user(trump_user, trump_pass, trump_part, trump_row);
    if (create_user_result != status_codes::OK){
      throw std::exception();
    }

    create_user_result = make_user(ted_user, ted_pass, ted_part, ted_row);
    if (create_user_result != status_codes::OK){
      throw std::exception();
    }

    create_user_result = make_user(kino_user, kino_pass, kino_part, kino_row);
    if (create_user_result != status_codes::OK){
      throw std::exception();
    }

    create_user_result = make_user(clinton_user, clinton_pass, clinton_part, clinton_row);
    if (create_user_result != status_codes::OK){
      throw std::exception();
    }

    create_user_result = make_ghost(phan_user, phan_pass, phan_part, phan_row);
    if (create_user_result != status_codes::OK){
      throw std::exception();
    }
}

  ~UserFixture() {
    int delete_user_result {delete_user(bob_user, bob_part, bob_row)};
    if(delete_user_result != status_codes::OK){
      throw std::exception();
    }

    delete_user_result = delete_user(baker_user, baker_part, baker_row);
    if(delete_user_result != status_codes::OK){
      throw std::exception();
    }

    delete_user_result = delete_user(trump_user, trump_part, trump_row);
    if(delete_user_result != status_codes::OK){
      throw std::exception();
    }

    delete_user_result = delete_user(ted_user, ted_part, ted_row);
    if(delete_user_result != status_codes::OK){
      throw std::exception();
    }

    delete_user_result = delete_user(kino_user, kino_part, kino_row);
    if(delete_user_result != status_codes::OK){
      throw std::exception();
    }

    delete_user_result = delete_user(clinton_user, clinton_part, clinton_row);
    if(delete_user_result != status_codes::OK){
      throw std::exception();
    }

	delete_user_result = delete_ghost(phan_user);
    if(delete_user_result != status_codes::OK){
      throw std::exception();
    }
  }
};

SUITE(USER_SERVER){
  TEST_FIXTURE(UserFixture, sign_on_and_off){
  	const string pwd_prop {UserFixture::auth_pwd_prop};
  	cout << endl << "TEST SCENARIO 1 (Bob's Signoff problem!!)" << endl;
    cout << "Bob from Zimbobwe decides to sign into Napbook" << endl;
    pair<status_code,value> result {
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::bob_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::bob_pass))})
                  )};
    CHECK_EQUAL(status_codes::OK, result.first);

    cout << "Bob got bored and signs off without doing anything" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::bob_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    cout << "Bob web browser lagged and he didn't see the signed off screen and decides to click sign off again" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::bob_user
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);


    cout << endl << "TEST SCENARIO 2 (Lying Ted...)" << endl;
    cout << "Ted tries to log onto Trump's account " << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::trump_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string("HillaryClinton"))})
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);


    cout << endl << "TEST SCENARIO 3 (Forgetful Baker Sensei)" << endl;
    cout << "Baker Sensei logins to Napbook to message her students about homework" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::baker_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::baker_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    cout << "She browses the web and forgets her current session on Napbook and tries to signin again" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::baker_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::baker_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    cout << "The page lags and she enters her password again but this time she accidently used '0' instead of 'o'" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::baker_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string("reds0x"))})
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);

    cout << "She forgets what she's doing and signs off" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::baker_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    cout << "She notices her other session (which is also signed off by now) and clicks signoff again" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::baker_user
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);


    cout << endl << "TEST SCENARIO 4 (A Ghost tries to signin)" << endl;
    cout << "A ghost decides to sign in to Napbook" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::phan_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::phan_pass))})
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);

    cout << endl << "TEST SCENARIO 5 (Everyone signs in all at once!)" << endl;
    cout << "Napbook's user base decides to sign in at the same time" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::bob_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::bob_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::baker_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::baker_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::trump_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::trump_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::ted_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::ted_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::kino_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::kino_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    cout << "Everyone decides to sign off at the same time and they all accidently press the button twice" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::bob_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::bob_user
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::baker_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::baker_user
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::trump_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::trump_user
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::ted_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::ted_user
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::kino_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::kino_user
                  );
    CHECK_EQUAL(status_codes::NotFound, result.first);
  }

  TEST_FIXTURE(UserFixture, add_unfriend_and_get_friendslist){
  	const string pwd_prop {UserFixture::auth_pwd_prop};

    cout << endl << "TEST SCENARIO 6 (Donald's Napbook adventures)" << endl;
    cout << "Trump tries to add friends from the Napbook app but forgets to sign in" << endl;

    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + UserFixture::ted_part + "/"
                  + UserFixture::ted_row
                  )};
    CHECK_EQUAL(status_codes::Forbidden, result.first);

    cout << "He realizes that and signs in and adds ted again" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::trump_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::trump_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);
    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + UserFixture::ted_part + "/"
                  + UserFixture::ted_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    cout << "He then adds his bff hillary to his friends list too" << endl;
    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + UserFixture::clinton_part + "/"
                  + UserFixture::clinton_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    cout << "He also adds his Ivanka, even though she has no account, because he's #BestFatherEva" << endl;
    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + "USA/"
                  + "Trump,Ivanka"
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::GET,
                  string(UserFixture::userserver_addr)
                  + read_friend_list_op + "/"
                  + UserFixture::trump_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    value expect {value::object(vector<pair<string,value>>{make_pair(string(UserFixture::friend_prop), value::string("Canada;Cruz,Ted|USA;Clinton,Hillary|USA;Trump,Ivanka"))})};
    compare_json_values (expect, result.second);

    cout << "Ted says some rude things about Donald (cuz he's lying ted) and Donald unfriends him" << endl;
    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + unfriend_op + "/"
                  + UserFixture::trump_user + "/"
                  + UserFixture::ted_part + "/"
                  + UserFixture::ted_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::GET,
                  string(UserFixture::userserver_addr)
                  + read_friend_list_op + "/"
                  + UserFixture::trump_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    expect = value::object(vector<pair<string,value>>{make_pair(string(UserFixture::friend_prop), value::string("USA;Clinton,Hillary|USA;Trump,Ivanka"))});
    compare_json_values (expect, result.second);

    cout << "Donald also unfriends jeb even though he was never his friend" << endl;
    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + unfriend_op + "/"
                  + UserFixture::trump_user + "/"
                  + "USA/"
                  + "Bush,Jeb"
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::GET,
                  string(UserFixture::userserver_addr)
                  + read_friend_list_op + "/"
                  + UserFixture::trump_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    expect = value::object(vector<pair<string,value>>{make_pair(string(UserFixture::friend_prop), value::string("USA;Clinton,Hillary|USA;Trump,Ivanka"))});
    compare_json_values (expect, result.second);

    cout << "Donald now signs off but forgets to unfriend Hillary and attempts to unfriend her without being online" << endl;

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::trump_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);
    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + unfriend_op + "/"
                  + UserFixture::trump_user + "/"
                  + UserFixture::clinton_part + "/"
                  + UserFixture::clinton_row
                  );
    CHECK_EQUAL(status_codes::Forbidden, result.first);

    cout << "Not only that but Trump tries to check his friends list without an active session" << endl;

    result =
      do_request (methods::GET,
                  string(UserFixture::userserver_addr)
                  + read_friend_list_op + "/"
                  + UserFixture::trump_user
                  );
    CHECK_EQUAL(status_codes::Forbidden, result.first);
  }

  TEST_FIXTURE(UserFixture, status_updates){
  	const string pwd_prop {UserFixture::auth_pwd_prop};

    cout << endl << "Preparing status update tests..." << endl;

  	pair<status_code,value> result {
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::bob_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::bob_pass))})
                  )};
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::baker_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::baker_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::trump_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::trump_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::ted_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::ted_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::kino_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::kino_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::clinton_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::clinton_pass))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    //adding friends...
    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + UserFixture::ted_part + "/"
                  + UserFixture::ted_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + UserFixture::clinton_part + "/"
                  + UserFixture::clinton_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + "USA/"
                  + "Trump,Ivanka"
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::clinton_user + "/"
                  + UserFixture::trump_part + "/"
                  + UserFixture::trump_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::ted_user + "/"
                  + UserFixture::trump_part + "/"
                  + UserFixture::trump_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::baker_user + "/"
                  + UserFixture::kino_part + "/"
                  + UserFixture::kino_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::kino_user + "/"
                  + UserFixture::baker_part + "/"
                  + UserFixture::baker_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);


    cout << endl << "TEST SCENARIO 6 (Donald loves updating his status)" << endl;

    cout << "Everyone logs into Napbook and updates their status" << endl;

    cout << "Donald goes crazy on Napbook to gain popularity in the election" << endl;
    const string trump_line_1 {"Make_America_Great_Again"};
    const string trump_line_2 {"Ted_is_a_giant_liar"};
    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + update_status_op + "/"
                  + UserFixture::trump_user + "/"
                  + trump_line_1
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result = 
      do_request (methods::GET, string(addr)
                  + read_entity_admin + "/"
                  + string(UserFixture::table)
                  , value::object(vector<pair<string,value>>{make_pair(string(UserFixture::status_prop), value::string(trump_line_1))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    CHECK_EQUAL(1, result.second.as_array().size());

    result = 
      do_request (methods::GET, string(addr)
                  + read_entity_admin + "/"
                  + string(UserFixture::table)
                  , value::object(vector<pair<string,value>>{make_pair(string(UserFixture::update_prop), value::string(trump_line_1))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    CHECK_EQUAL(2, result.second.as_array().size());

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + update_status_op + "/"
                  + UserFixture::trump_user + "/"
                  + trump_line_2
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result = 
      do_request (methods::GET, string(addr)
                  + read_entity_admin + "/"
                  + string(UserFixture::table)
                  , value::object(vector<pair<string,value>>{make_pair(string(UserFixture::status_prop), value::string(trump_line_2))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    CHECK_EQUAL(1, result.second.as_array().size());

    result = 
      do_request (methods::GET, string(addr)
                  + read_entity_admin + "/"
                  + string(UserFixture::table)
                  , value::object(vector<pair<string,value>>{make_pair(string(UserFixture::update_prop), value::string(trump_line_1 + "\n" + trump_line_2))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(2, result.second.as_array().size());

    cout << endl << "TEST SCENARIO 7 (Baker sensei's confession)" << endl;


    cout << "Ms Baker decides to update her status" << endl;

    string BakerLine1 {"My_favorite_team_is_the_Boston_Red_Sox"};

    result =
      do_request (methods::PUT, string(UserFixture::userserver_addr)
                  + update_status_op + "/"
                  + UserFixture::baker_user + "/"
                  + BakerLine1 );
    CHECK_EQUAL(status_codes::OK, result.first);

    result = 
      do_request (methods::GET, string(addr)
                  + read_entity_admin + "/"
                  + string(UserFixture::table)
                  , value::object(vector<pair<string,value>>{make_pair(string(UserFixture::status_prop), value::string(BakerLine1))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    CHECK_EQUAL(1, result.second.as_array().size());

    result = 
      do_request (methods::GET, string(addr)
                  + read_entity_admin + "/"
                  + string(UserFixture::table)
                  , value::object(vector<pair<string,value>>{make_pair(string(UserFixture::update_prop), value::string(BakerLine1))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(1, result.second.as_array().size());

    cout << "She logs off but forgets that she's offline and tries to update again" << endl;
    result =
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_off_op + "/"
                  + UserFixture::baker_user
                  );
    CHECK_EQUAL(status_codes::OK, result.first);


    string BakerLine2 = "Remember_to_do_your_English_homework";

    result =
      do_request (methods::PUT, string(UserFixture::userserver_addr)
                  + update_status_op + "/"
                  + UserFixture::baker_user + "/"
                  + BakerLine2 );
    CHECK_EQUAL(status_codes::Forbidden, result.first);
  }

  TEST_FIXTURE(UserFixture, UserDisallowedMethod)
  {
    cout << endl << "TEST SCENARIO 8 (Hacker tries to delete something with userserver)" << endl;
    cout << "random hacker decides to hack into Napbook" << endl;
    pair<status_code,value> result {
      do_request (methods::DEL,
                userserver_addr + delete_entity_admin + "/" + table + "/" + ted_part + "/" + ted_row
    )};
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);
  }

  TEST_FIXTURE(UserFixture, UserMalformedRequest)
  {
    const string do_something_op {"DoSomething"};
    cout << endl << "TEST SCENARIO 9 (Hacker tries bad command with userserver)" << endl;
    cout << "random hacker decides to hack into Napbook" << endl;
    cout << "He first tries to use post" << endl;
    pair<status_code,value> result {
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + do_something_op
                  )};
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    cout << "Afterwards he tries put" << endl;
    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + do_something_op
                  );
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    cout << "With a last ditch effort he tries get" << endl;
    result =
      do_request (methods::GET,
                  string(UserFixture::userserver_addr)
                  + do_something_op
                  );
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }
}
SUITE(PUSH_SERVER){
  TEST_FIXTURE(UserFixture, push_status){
    const string pwd_prop {UserFixture::auth_pwd_prop};

    cout << endl << "Quick push server test cuz user server already does this..." << endl;
    cout << "Preparing push tests..." << endl;

    pair<status_code,value> result {
      do_request (methods::POST,
                  string(UserFixture::userserver_addr)
                  + sign_on_op + "/"
                  + UserFixture::trump_user,
                  value::object (vector<pair<string,value>>
                                   {make_pair(pwd_prop ,
                                              value::string(UserFixture::trump_pass))})
                  )};

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + UserFixture::ted_part + "/"
                  + UserFixture::ted_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + UserFixture::clinton_part + "/"
                  + UserFixture::clinton_row
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    const string trump_line_1 {"Make_America_Great_Again"};

    cout << "Testing with everyone existing" << endl;
    pair<status_code,value> list_result {
      do_request (methods::GET,
                  string(UserFixture::userserver_addr)
                  + read_friend_list_op + "/"
                  + UserFixture::trump_user
                  )};
    CHECK_EQUAL(status_codes::OK, list_result.first);

    value friend_list {list_result.second};

    result =
      do_request (methods::POST,
                  string(UserFixture::push_addr)
                  + push_status_op + "/"
                  + UserFixture::trump_part + "/"
                  + UserFixture::trump_row + "/"
                  + trump_line_1
                  , friend_list
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result = 
      do_request (methods::GET, string(addr)
                  + read_entity_admin + "/"
                  + string(UserFixture::table)
                  , value::object(vector<pair<string,value>>{make_pair(string(UserFixture::update_prop), value::string(trump_line_1))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    CHECK_EQUAL(2, result.second.as_array().size());

    cout << "Testing with non-existant ppl and already have one update" << endl;

    result =
      do_request (methods::PUT,
                  string(UserFixture::userserver_addr)
                  + add_friend_op + "/"
                  + UserFixture::trump_user + "/"
                  + "USA/"
                  + "Trump,Ivanka"
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    list_result =
      do_request (methods::GET,
                  string(UserFixture::userserver_addr)
                  + read_friend_list_op + "/"
                  + UserFixture::trump_user
                  );
    CHECK_EQUAL(status_codes::OK, list_result.first);

    friend_list = list_result.second;

    const string trump_line_2 {"Ted_is_a_giant_liar"};

    result =
      do_request (methods::POST,
                  string(UserFixture::push_addr)
                  + push_status_op + "/"
                  + UserFixture::trump_part + "/"
                  + UserFixture::trump_row + "/"
                  + trump_line_2
                  , friend_list
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    result = 
      do_request (methods::GET, string(addr)
                  + read_entity_admin + "/"
                  + string(UserFixture::table)
                  , value::object(vector<pair<string,value>>{make_pair(string(UserFixture::update_prop), value::string(trump_line_1 + "\n" + trump_line_2))})
                  );
    CHECK_EQUAL(status_codes::OK, result.first);

    CHECK_EQUAL(2, result.second.as_array().size());
  }

  TEST_FIXTURE(UserFixture, PushDisallowedMethod)
  {
    cout << endl << "quick test on disallowed method for push server" << endl;

    const string do_something_op {"DoSomething"};

    pair<status_code,value> result {
      do_request (methods::DEL,
                push_addr + do_something_op
    )};
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);

    result = 
      do_request (methods::GET,
                push_addr + do_something_op
    );
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);

    result = 
      do_request (methods::PUT,
                push_addr + do_something_op
    );
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);
  }

  TEST_FIXTURE(UserFixture, PushMalformedRequest)
  {
    const string do_something_op {"DoSomething"};
    cout << endl << "quick test on malformed requests" << endl;
    pair<status_code,value> result {
      do_request (methods::POST,
                  string(UserFixture::push_addr)
                  + do_something_op
                  )};
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }

}
// End of our extensions ================================================================================================================

