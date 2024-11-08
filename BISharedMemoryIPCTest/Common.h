#pragma once

#ifndef BOOST_INTERPROCESS_FORCE_NATIVE_EMULATION
#define BOOST_INTERPROCESS_FORCE_NATIVE_EMULATION
#endif

#include <chrono>
#include <iostream>
#include <boost/unordered_map.hpp>
#include <boost/container/scoped_allocator.hpp>
#include <boost/container/string.hpp>
#include <boost/interprocess/managed_windows_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/containers/flat_map.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/lockfree/spsc_queue.hpp>

using namespace boost::interprocess;

constexpr const char* SHARED_MEMORY_NAME = "3676FC03-9EF0-450E-920A-873723677AC9";
constexpr const char* NEW_REQ_MUTEX_NAME = "NEW_REQ_MUTEX";
constexpr const char* NEW_REQ_EVENT_NAME = "NEW_REQ_EVENT";
constexpr const char* REQ_MUTEX_NAME = "REQ_MUTEX";
constexpr const char* REQ_NAME = "REQ_QUEUE";

constexpr const char* DATAACCESS_SERVICE = "DATAACCESS";
constexpr const char* DATAACCESS_SIMPLE_FUNCTION = "SIMPLE_FUNCTION";
constexpr const char* DATAACCESS_SET_VALUES = "SET_VALUES";
constexpr const char* DATAACCESS_GET_VALUES = "GET_VALUES";

using custom_shared_memory = basic_managed_windows_shared_memory<char,
	rbtree_best_fit<
	mutex_family,
	offset_ptr<
	void,
	__int64,
	unsigned __int64,
	0>,
	0>,
	iset_index>;

//Typedefs of allocators and containers
typedef custom_shared_memory::segment_manager													segment_manager_t;
typedef allocator<void, segment_manager_t>														void_allocator;

typedef allocator<int, segment_manager_t>														int_allocator;
typedef vector<int, int_allocator>																int_vector;
typedef allocator<int_vector, segment_manager_t>												int_vector_allocator;

typedef allocator<char, segment_manager_t>														char_allocator;
typedef basic_string<char, std::char_traits<char>, char_allocator>								char_string;
typedef allocator<char_string, segment_manager_t>												char_string_allocator;

typedef vector<char_string, char_string_allocator>												string_vector;

typedef std::pair<char_string, double>															tagvalue;
typedef allocator<tagvalue, segment_manager_t>													tagvalue_allocator;
typedef vector<tagvalue, boost::container::scoped_allocator_adaptor<tagvalue_allocator>>        tagvalue_vector;

struct Reply
{
	interprocess_mutex ready_mutex;
	interprocess_condition ready;
	offset_ptr<void> data;
};

struct Request
{
	using allocator_type = void_allocator;

	long long time;
	char_string service;
	char_string function;
	offset_ptr<void> data;
	offset_ptr<Reply> reply;

	Request(const char *service, const char *function, offset_ptr<void> data, offset_ptr<Reply> reply, const void_allocator &alloc) :
		service{ service, alloc },
		function{ function, alloc },
		data{ data },
		reply{ reply }
	{
		time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	Request(const void_allocator &alloc) :
		time{ 0 },
		service{ alloc },
		function{ alloc },
		data{ nullptr },
		reply{ nullptr }
	{

	}
};

using r_allocator = allocator<Request, segment_manager_t>;

//change to multiple writer and reader to support multi-threading?
using request_ringbuffer = boost::lockfree::spsc_queue<
	Request,
	boost::lockfree::capacity<200>>;