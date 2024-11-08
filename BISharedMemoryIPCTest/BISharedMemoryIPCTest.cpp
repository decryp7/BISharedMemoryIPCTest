// BISharedMemoryIPCTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#ifndef BOOST_INTERPROCESS_FORCE_NATIVE_EMULATION
#define BOOST_INTERPROCESS_FORCE_NATIVE_EMULATION
#endif

#include <chrono>
#include <thread>
#include <iostream>
#include <boost/interprocess/managed_windows_shared_memory.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/string.hpp>
#include <boost/variant.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include "Common.h"
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <boost/bind.hpp>

int main()
{
	using namespace std::chrono_literals;
	using namespace boost::interprocess;

	custom_shared_memory *shm = nullptr;

	try {
		std::cout << "Connecting to server...\n";
		shm = new custom_shared_memory(open_only, SHARED_MEMORY_NAME);
		interprocess_mutex *new_req_mutex = shm->find<interprocess_mutex>(NEW_REQ_MUTEX_NAME).first;
		interprocess_condition *new_req_event = shm->find<interprocess_condition>(NEW_REQ_EVENT_NAME).first;
		interprocess_mutex *req_queue_mutex = shm->find<interprocess_mutex>(REQ_MUTEX_NAME).first;
		request_ringbuffer *request_queue = shm->find<request_ringbuffer>(REQ_NAME).first;
		void_allocator shm_alloc(shm->get_segment_manager());
		segment_manager_t *segment_manager = shm->get_segment_manager();

		while (true) {
			Sleep(1000);

			{
				//SimpleFunction
				auto start = std::chrono::high_resolution_clock::now();
				Reply *rp = shm->construct<Reply>(anonymous_instance)();
				scoped_lock<interprocess_mutex> lock(rp->ready_mutex);

				//1. Create payload.
				void *p = shm->construct<char_string>(anonymous_instance)("Hello World!", segment_manager);

				//2. Create and send the request. Logically possible to send request on another thread but queue needs to change to multiple writer and reader.
				//Possible to keep single writer/reader (spsc_queue)?
				Request r{ DATAACCESS_SERVICE,
							DATAACCESS_SIMPLE_FUNCTION,
							p,
							rp,
							shm_alloc };
				{
					//std::cout << "Send SimpleFunction request.\n";
					scoped_lock<interprocess_mutex> req_lock(*req_queue_mutex);
					request_queue->push(r);
					scoped_lock<interprocess_mutex> notify_lock(*new_req_mutex);
					new_req_event->notify_all();
				}

				//3. Wait for the reply and read the reply handle to get the result from shared memory.
				cv_status status = rp->ready.wait_until(lock, std::chrono::system_clock::now() + 1s);

				int result = 1;
				if (status == cv_status::timeout)
				{
					//std::cout << "Wait SimpleFunction request timeout.\n";
					result = 2;
				}
				else
				{
					//std::cout << "Read SimpleFunction reply.\n";
					//4. Get reply using handle from shared memory
					result = *(static_cast<int*>(rp->data.get()));
				}

				shm->destroy_ptr(rp);
				shm->destroy_ptr(p);
			}
			std::cout << "Free memory: " << shm->get_free_memory() / 1000 / 1000 << "mb.\n";
		}
	}
	catch (std::exception e)
	{
		//std::cout << e.get_error_code() << std::endl;
		std::cout << "Start Server..." << std::endl;
		shm = new custom_shared_memory(open_or_create, SHARED_MEMORY_NAME, 100 * 1000 * 1024);
		interprocess_mutex *new_req_mutex = shm->find_or_construct<interprocess_mutex>(NEW_REQ_MUTEX_NAME)();
		interprocess_condition *new_req_event = shm->find_or_construct<interprocess_condition>(NEW_REQ_EVENT_NAME)();
		interprocess_mutex *req_queue_mutex = shm->find_or_construct<interprocess_mutex>(REQ_MUTEX_NAME)();
		void_allocator shm_alloc(shm->get_segment_manager());
		segment_manager_t *segment_manager = shm->get_segment_manager();
		request_ringbuffer *req_queue = shm->find_or_construct<request_ringbuffer>(REQ_NAME)();

		while (true) {
			{
				scoped_lock<interprocess_mutex> lock{ *new_req_mutex };
				new_req_event->wait(lock);
			}

			Request r{ shm_alloc };

			do
			{
				if (r.function == DATAACCESS_SIMPLE_FUNCTION)
				{
					//std::cout << "Received simple function request.\n";
					//2. Get the payload.
					Reply *rp = r.reply.get();
					char_string *p = static_cast<char_string*>(r.data.get());
					//printf("param: %s\n", p->c_str());

					//4. Store the reply on shared memory and set the handle.
					rp->data = shm->construct<int>(anonymous_instance)(0);

					//4. Store the reply on shared memory and set the handle.
					scoped_lock<interprocess_mutex> lock(rp->ready_mutex);
					//std::cout << "Send simple function reply.\n";
					rp->ready.notify_all();
				}

				std::cout << "Free memory: " << shm->get_free_memory() / 1000 / 1000 << "mb\n";

			}while([&]
			{
				bool has_request{ false };
				{
					scoped_lock<interprocess_mutex> req_lock{ *req_queue_mutex };
					has_request = req_queue->pop(r);
				}
				return has_request;
			}());
		}
	}

	system("pause");
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
