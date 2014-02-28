/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
/*
 * producer.cpp
 *
 *  Created on: 21 Jun 2011
 *      Author: Ben Gray (@benjamg)
 */

#include <boost/lexical_cast.hpp>

#include "producer.hpp"

namespace kafka {

producer::producer(const compression_type compression, boost::asio::io_service& io_service)
	: _io_service(io_service)
	, _connected(false)
	, _connecting(false)
	, _compression(compression)
	, _resolver(io_service)
	, _socket(new boost::asio::ip::tcp::socket(io_service))
{
}

producer::~producer()
{
	close();
}

bool producer::connect(std::string const& hostname
		, uint16_t const port
		, connect_error_handler_function const & error_handler)
{
	return connect(hostname, boost::lexical_cast<std::string>(port), error_handler);
}

bool producer::connect(std::string const& hostname
		, std::string const& servicename
		, connect_error_handler_function const & error_handler)
{
	if (_connecting) { return false; }
	_connecting = true;

	boost::asio::ip::tcp::resolver::query query(hostname, servicename);
	_resolver.async_resolve(
		query,
		boost::bind(
			&producer::handle_resolve
				, this
				, boost::asio::placeholders::error
				, boost::asio::placeholders::iterator
				, error_handler
		)
	);

	return true;
}

bool producer::close()
{
	if (_connecting) { return false; }

	_connected = false;
	_socket->close();
	_socket.reset(new boost::asio::ip::tcp::socket(_io_service));

	return true;
}

bool producer::is_connected() const
{
	return _connected;
}

bool producer::is_connecting() const
{
	return _connecting;
}

void producer::handle_resolve(const boost::system::error_code& error_code
		, boost::asio::ip::tcp::resolver::iterator endpoints
		, connect_error_handler_function const & error_handler)
{
	if (!error_code)
	{
		boost::asio::ip::tcp::endpoint endpoint = *endpoints;
		_socket->async_connect(
			endpoint
			, boost::bind(
					&producer::handle_connect
					, this
					, boost::asio::placeholders::error
					, ++endpoints
					, error_handler
					)
			);
	}
	else
	{
		_connecting = false;
		if (error_handler.empty())
		{
			throw boost::system::system_error(error_code);
		} else {
			error_handler(error_code);
		}
	}
}

void producer::handle_connect(const boost::system::error_code& error_code
		, boost::asio::ip::tcp::resolver::iterator endpoints
		, connect_error_handler_function const & error_handler)
{
	// this check needs a resolution of boost https://svn.boost.org/trac/boost/ticket/8795
	if (!error_code)
	{
		// The connection was successful.
		_connecting = false;
		_connected = true;

		// start a read that will tell us the connection has closed
		std::shared_ptr<boost::array<char, 1>> buf(new boost::array<char, 1>);
		boost::asio::async_read(*_socket
					, boost::asio::buffer(*buf)
					, boost::bind(&producer::handle_dummy_read
									, this
									, buf
									, boost::asio::placeholders::error
								  )
					);
	}
	else if (endpoints != boost::asio::ip::tcp::resolver::iterator())
	{
		// TODO: handle connection error (we might not need this as we have others though?)

		// The connection failed, but we have more potential endpoints so throw it back to handle resolve
		handle_resolve(boost::system::error_code(), endpoints, error_handler);
	}
	else
	{
		_connecting = false;
		if (error_handler.empty())
		{
			throw boost::system::system_error(error_code);
		} else {
			error_handler(error_code);
		}
	}
}

void producer::handle_write_request(const boost::system::error_code& error_code
		, std::size_t /*bytes_transferred*/
		, message_ptr_t msg_ptr
		, boost::shared_ptr<boost::asio::streambuf> data
		, send_error_handler_function const & error_handler)
{

   	if (error_code)
	{
		if (error_handler.empty())
		{
			throw boost::system::system_error(error_code);
		} else
		{
			error_handler(error_code, msg_ptr);
		}
	}
}

}
