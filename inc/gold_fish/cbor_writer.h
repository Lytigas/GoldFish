#pragma once

#include <exception>
#include "array_ref.h"
#include "debug_checks_writer.h"
#include <limits>
#include "numbers.h"
#include "sax_writer.h"

namespace gold_fish { namespace cbor
{
	namespace details
	{
		template <uint8_t major, class Stream> void write_integer(Stream& s, uint64_t x)
		{
			if (x <= 23)
			{
				stream::write(s, static_cast<uint8_t>((major << 5) | x));
			}
			else if (x <= std::numeric_limits<uint8_t>::max())
			{
				stream::write(s, static_cast<uint16_t>((major << 5) | 24 | (x << 8)));
			}
			else if (x <= std::numeric_limits<uint16_t>::max())
			{
				stream::write(s, static_cast<uint8_t>((major << 5) | 25));
				stream::write(s, byte_swap(static_cast<uint16_t>(x)));
			}
			else if (x <= std::numeric_limits<uint32_t>::max())
			{
				stream::write(s, static_cast<uint8_t>((major << 5) | 26));
				stream::write(s, byte_swap(static_cast<uint32_t>(x)));
			}
			else
			{
				stream::write(s, static_cast<uint8_t>((major << 5) | 27));
				stream::write(s, byte_swap(x));
			}
		}
	}

	template <class Stream> class stream_writer
	{
	public:
		stream_writer(Stream& s)
			: m_stream(s)
		{}
		void write_buffer(const_buffer_ref buffer)
		{
			m_stream.write_buffer(buffer);
		}
		void flush() { }
	private:
		Stream& m_stream;
	};

	template <class Stream, uint8_t major> class indefinite_stream_writer
	{
	public:
		indefinite_stream_writer(Stream& s)
			: m_stream(s)
		{}
		void write_buffer(const_buffer_ref buffer)
		{
			details::write_integer<major>(m_stream, buffer.size());
			m_stream.write_buffer(buffer);
		}
		void flush()
		{
			stream::write(m_stream, static_cast<uint8_t>(0xFF));
		}
	private:
		Stream& m_stream;
	};

	template <class Stream> class array_writer;
	template <class Stream> class indefinite_array_writer;
	template <class Stream> class map_writer;
	template <class Stream> class indefinite_map_writer;

	template <class Stream> class document_writer
	{
	public:
		document_writer(Stream& s)
			: m_stream(s)
		{}
		void write(bool x)
		{
			if (x) stream::write(m_stream, static_cast<uint8_t>((7 << 5) | 21));
			else   stream::write(m_stream, static_cast<uint8_t>((7 << 5) | 20));
		}
		void write(nullptr_t)
		{
			stream::write(m_stream, static_cast<uint8_t>((7 << 5) | 22));
		}
		void write(double x)
		{
			stream::write(m_stream, static_cast<uint8_t>((7 << 5) | 27));
			auto i = *reinterpret_cast<uint64_t*>(&x);
			stream::write(m_stream, byte_swap(i));
		}
		void write_undefined() 
		{
			stream::write(m_stream, static_cast<uint8_t>((7 << 5) | 23));
		}

		void write(uint64_t x) { details::write_integer<0>(m_stream, x); }
		void write(int64_t x) { details::write_integer<1>(m_stream, static_cast<uint64_t>(-1ll - x)); }
		void write(uint32_t x) { details::write_integer<0>(m_stream, x); }
		void write(int32_t x) { details::write_integer<1>(m_stream, static_cast<uint64_t>(-1ll - x)); }
		stream_writer<Stream> write_binary(uint64_t cb)
		{
			details::write_integer<2>(m_stream, cb);
			return{ m_stream };
		}
		stream_writer<Stream> write_text(uint64_t cb)
		{
			details::write_integer<3>(m_stream, cb);
			return{ m_stream };
		}
		indefinite_stream_writer<Stream, 2> write_binary()
		{
			details::write_integer<2>(m_stream, 31);
			return{ m_stream };
		}
		indefinite_stream_writer<Stream, 3> write_text()
		{
			details::write_integer<3>(m_stream, 31);
			return{ m_stream };
		}

		array_writer<Stream> write_array(uint64_t size);
		indefinite_array_writer<Stream> write_array();

		map_writer<Stream> write_map(uint64_t size);
		indefinite_map_writer<Stream> write_map();

		template <class Document> std::enable_if_t<tags::has_tag<Document, tags::document>::value, void> write(Document& d)
		{
			copy_document(*this, d);
		}

	private:
		Stream& m_stream;
	};
	template <class Stream> document_writer<Stream> write_no_debug_check(Stream& s)
	{
		return{ s };
	}
	template <class Stream> auto write(Stream& s)
	{
		return debug_check::add_write_checks(write_no_debug_check(s));
	}
	
	template <class Stream> class array_writer
	{
	public:
		array_writer(Stream& s)
			: m_stream(s)
		{}

		document_writer<Stream> append() { return{ m_stream }; }
		void flush() {}
	private:
		Stream& m_stream;
	};
	template <class Stream> class indefinite_array_writer
	{
	public:
		indefinite_array_writer(Stream& s)
			: m_stream(s)
		{}
		document_writer<Stream> append() { return{ m_stream }; }
		void flush() { stream::write(m_stream, static_cast<uint8_t>(0xFF)); }
	private:
		Stream& m_stream;
	};
	template <class Stream> array_writer<Stream> document_writer<Stream>::write_array(uint64_t size)
	{
		details::write_integer<4>(m_stream, size);
		return{ m_stream };
	}
	template <class Stream> indefinite_array_writer<Stream> document_writer<Stream>::write_array()
	{
		details::write_integer<4>(m_stream, 31);
		return{ m_stream };
	}

	template <class Stream> class map_writer
	{
	public:
		map_writer(Stream& s)
			: m_stream(s)
		{}

		document_writer<Stream> append_key() { return{ m_stream }; }
		document_writer<Stream> append_value() { return{ m_stream }; }
		void flush() {}
	private:
		Stream& m_stream;
	};
	template <class Stream> class indefinite_map_writer
	{
	public:
		indefinite_map_writer(Stream& s)
			: m_stream(s)
		{}

		document_writer<Stream> append_key() { return{ m_stream }; }
		document_writer<Stream> append_value() { return{ m_stream }; }
		void flush() { stream::write(m_stream, static_cast<uint8_t>(0xFF)); }
	private:
		Stream& m_stream;
	};
	template <class Stream> map_writer<Stream> document_writer<Stream>::write_map(uint64_t size)
	{
		details::write_integer<5>(m_stream, size);
		return{ m_stream };
	}
	template <class Stream> indefinite_map_writer<Stream> document_writer<Stream>::write_map()
	{
		details::write_integer<5>(m_stream, 31);
		return{ m_stream };
	}
}}