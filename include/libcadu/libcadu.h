#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <span>
#include <memory>
#include <optional>
#include <vector>
#include <utility>

extern "C" {
#include "fec.h"
}

#include "libgetsetproxy/proxy.h"

// TODO: move everything that's not the CADU into the CADU

// TODO: ensure that any output CADUs don't have their first_header_pointer beyond the frame
//  Maybe emit a warning in this case?

// TODO: implement is_fill test on CADU
// TODO: Precalculate array of uint8_ts of sync pulse middle sections - 8x3
// As constexpr into a const buffer
// Currently we rely on the bit pattern being synchronised already

// TODO: test bit outputs
// TODO: calculate the sync marker most significant bit version by byte shifting in constexpr
namespace cadu {
  constexpr uint32_t SYNC_MARKER = 0x1acffc1d;
  constexpr uint32_t SYNC_MARKER_MSB = 0x1dfccf1a;
  constexpr int VERSION_NUMBER_LEN = 2;
  constexpr int SCID_LEN = 8;
  constexpr int VCID_LEN = 6;
  constexpr int VCDU_COUNTER_LEN = 24;
  constexpr int REPLAY_FLAG_LEN = 1;
  constexpr int VCDU_SPARE_LEN = 7;
  constexpr int M_PDU_SPARE_LEN = 5;
  constexpr int FIRST_HEADER_POINTER_LEN = 11;
  constexpr int DATA_LEN = 884;
}

// Default encoding table, from generator polynomial x**8 + x**7 + x**5 + x**3 + 1
// From gov/nasa/gsfc/drl/rtstps/core/PnDecoder.java, originally from
// Gerald Grebowsky of GSFC in 1996

const uint8_t randomise_table[] = {
    0xff, 0x48, 0x0e, 0xc0, 0x9a, 0x0d, 0x70, 0xbc, 0x8e, 0x2c, 0x93,
    0xad, 0xa7, 0xb7, 0x46, 0xce, 0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2,
    0xbf, 0x3e, 0x0a, 0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1, 0xfe,
    0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79, 0x1c, 0x59, 0x27, 0x5b,
    0x4f, 0x6e, 0x8d, 0x9c, 0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e,
    0x7c, 0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63, 0xfd, 0x20,
    0x3b, 0x02, 0x68, 0x35, 0xc2, 0xf2, 0x38, 0xb2, 0x4e, 0xb6, 0x9e,
    0xdd, 0x1b, 0x39, 0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8,
    0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7, 0xfa, 0x40, 0x76,
    0x04, 0xd0, 0x6b, 0x85, 0xe4, 0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba,
    0x36, 0x72, 0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0, 0x50,
    0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f, 0xf4, 0x80, 0xec, 0x09,
    0xa0, 0xd7, 0x0b, 0xc8, 0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c,
    0xe5, 0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0, 0xa1, 0x0f,
    0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f, 0xe9, 0x01, 0xd8, 0x13, 0x41,
    0xae, 0x17, 0x91, 0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb,
    0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1, 0x42, 0x1e, 0x31,
    0x12, 0x99, 0xbd, 0x56, 0x3f, 0xd2, 0x03, 0xb0, 0x26, 0x83, 0x5c,
    0x2f, 0x23, 0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96, 0xa5,
    0xdf, 0x73, 0x0c, 0xa8, 0xaf, 0xcf, 0x82, 0x84, 0x3c, 0x62, 0x25,
    0x33, 0x7a, 0xac, 0x7f, 0xa4, 0x07, 0x60, 0x4d, 0x06, 0xb8, 0x5e,
    0x47, 0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d, 0x4b, 0xbe,
    0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x05, 0x08, 0x78, 0xc4, 0x4a, 0x66,
    0xf5, 0x58
};


// TODO: methods to allow the insertion of spans into the data

#pragma pack(push, 1)
struct VC_PDU {
  friend class CADU;
private:
  uint16_t _scid_h : cadu::SCID_LEN - 2 = 0;
  uint16_t _version_number : cadu::VERSION_NUMBER_LEN = 0;
  uint16_t _vcid : cadu::VCID_LEN = 0;
  uint16_t _scid_l : 2 = 0;
  uint32_t _vcdu_counter_h : 8 = 0;
  uint32_t _vcdu_counter_m : 8 = 0;
  uint32_t _vcdu_counter_l : 8 = 0;
  uint32_t _vcdu_spare : cadu::VCDU_SPARE_LEN = 0;
  uint32_t _replay_flag : cadu::REPLAY_FLAG_LEN = 0;
  uint16_t _first_header_pointer_h : cadu::FIRST_HEADER_POINTER_LEN - 8 = 0;
  uint16_t _m_pdu_spare : cadu::M_PDU_SPARE_LEN = 0;
  uint16_t _first_header_pointer_l : 8 = 0;
  std::array<std::byte, cadu::DATA_LEN> _data = {};
};
#pragma pack(pop)
static_assert(std::is_trivially_copyable_v<VC_PDU>,
              "VC_PDU is not trivially copyable");
static_assert(std::is_standard_layout_v<VC_PDU>,
              "VC_PDU is not a standard layout type");


#pragma pack(push, 1)
struct CVCDU {
  friend class CADU;
private:
  VC_PDU vc_pdu;
  mutable std::array<std::byte, 128> _checksum;

public:
  CVCDU() = default;
  CVCDU(VC_PDU const &vc_pdu) : vc_pdu{vc_pdu} {}
};
#pragma pack(pop)
static_assert(sizeof(CVCDU) == 1020,
              "CVCDU is not of size 1020 as in the spec");
static_assert(std::is_trivially_copyable_v<CVCDU>,
              "CVCDU is not trivially copyable");
static_assert(std::is_standard_layout_v<CVCDU>,
              "CVCDU is not a standard layout type");

struct CADU;

namespace nonrandomised {
  auto operator<<(std::ostream & output, CADU const & cadu) -> std::ostream &;
  auto operator>>(std::istream & input, CADU & cadu) -> std::istream &;
}

namespace randomised {
  auto operator<<(std::ostream & output, CADU const & cadu) -> std::ostream &;
  auto operator>>(std::istream & input, CADU & cadu) -> std::istream &;
}

struct CADU {
private:
  struct Impl {
    const uint32_t sync = cadu::SYNC_MARKER_MSB;
    CVCDU cvcdu;

    Impl() = default;
    Impl(VC_PDU const &vc_pdu) : cvcdu{vc_pdu} {}
  };

  Impl impl;
  mutable bool dirty_checksum = true; // Even the empty CADU needs to have its checksum calculated

  auto randomise() -> void {
    auto data = reinterpret_cast<uint8_t*>(&impl.cvcdu);
    for (int i = 0; i < sizeof(impl.cvcdu); i++) {
      data[i] ^= randomise_table[i%sizeof(randomise_table)];
    }
  }

public:
  // Constructor for everything without sync pulse
  CADU() = default;
  CADU(const CADU &cadu) : impl{cadu.impl.cvcdu.vc_pdu}, dirty_checksum{cadu.dirty_checksum} {}
  CADU(uint8_t const *const input) {std::memcpy(&impl.cvcdu, input, sizeof(CVCDU));}

  CADU(VC_PDU const &vc_pdu) : impl{vc_pdu}, dirty_checksum{true} {}

private:
  void calculate_checksum(std::array<std::byte, 128>& checksum) const {
    // Deinterleave the blocks to depth 4
    // Described in sec 4.4.1 https://public.ccsds.org/Pubs/131x0b3e1.pdf
    auto data0 = std::array<unsigned char, 223> {};
    auto data1 = std::array<unsigned char, 223> {};
    auto data2 = std::array<unsigned char, 223> {};
    auto data3 = std::array<unsigned char, 223> {};

    static_assert(sizeof(impl.cvcdu.vc_pdu) == 4*223, "VC_PDU wrong size");
    auto buffer = reinterpret_cast<const unsigned char*>(&impl.cvcdu.vc_pdu);
    for (int i=222; i>-1; i--) {
      data0[i] = buffer[4*i];
      data1[i] = buffer[4*i+1];
      data2[i] = buffer[4*i+2];
      data3[i] = buffer[4*i+3];
    }

    auto parity0 = std::array<unsigned char, 32> {};
    auto parity1 = std::array<unsigned char, 32> {};
    auto parity2 = std::array<unsigned char, 32> {};
    auto parity3 = std::array<unsigned char, 32> {};

    encode_rs_ccsds(data0.data(), parity0.data(), 0);
    encode_rs_ccsds(data1.data(), parity1.data(), 0);
    encode_rs_ccsds(data2.data(), parity2.data(), 0);
    encode_rs_ccsds(data3.data(), parity3.data(), 0);

    for (int i=0; i<32; i++) {
      checksum[4*i] = std::byte(parity0[i]);
      checksum[4*i+1] = std::byte(parity1[i]);
      checksum[4*i+2] = std::byte(parity2[i]);
      checksum[4*i+3] = std::byte(parity3[i]);
    }
  }

public:
  auto version_number() const & {
    return static_cast<int>(impl.cvcdu.vc_pdu._version_number);
  }

  auto version_number() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).version_number(); },
      [this](int x) {
        impl.cvcdu.vc_pdu._version_number = x;
        dirty_checksum = true;
      }
    };
  }

  auto scid() const & {
    return static_cast<int>(impl.cvcdu.vc_pdu._scid_h << 2 | impl.cvcdu.vc_pdu._scid_l);
  }

  auto scid() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).scid(); },
      [this](int x) {
        impl.cvcdu.vc_pdu._scid_h = (x >> 2) & 0xff;
        impl.cvcdu.vc_pdu._scid_l = x & 0x03;
        dirty_checksum = true;
      }
    };
  }

  auto vcid() const & {
    return static_cast<int>(impl.cvcdu.vc_pdu._vcid);
  }

  auto vcid() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).vcid(); },
      [this](int x) {
        impl.cvcdu.vc_pdu._vcid = x;
        dirty_checksum = true;
      }
    };
  }

  auto vcdu_counter() const & {
    return static_cast<int>(impl.cvcdu.vc_pdu._vcdu_counter_h << 16 | impl.cvcdu.vc_pdu._vcdu_counter_m << 8 | impl.cvcdu.vc_pdu._vcdu_counter_l);
  }

  auto vcdu_counter() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).vcdu_counter(); },
      [this](int x) {
        impl.cvcdu.vc_pdu._vcdu_counter_h = (x >> 16) & 0xff;
        impl.cvcdu.vc_pdu._vcdu_counter_m = (x >> 8) & 0xff;
        impl.cvcdu.vc_pdu._vcdu_counter_l = x & 0xff;
        dirty_checksum = true;
      }
    };
  }

  auto replay_flag() const & {
    return static_cast<int>(impl.cvcdu.vc_pdu._replay_flag);
  }

  auto replay_flag() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).replay_flag(); },
      [this](int x) {
        impl.cvcdu.vc_pdu._replay_flag = x;
        dirty_checksum = true;
      }
    };
  }

  auto vcdu_spare() const & {
    return static_cast<int>(impl.cvcdu.vc_pdu._vcdu_spare);
  }

  auto vcdu_spare() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).vcdu_spare(); },
      [this](int x) {
        impl.cvcdu.vc_pdu._vcdu_spare = x;
        dirty_checksum = true;
      }
    };
  }

  auto m_pdu_spare() const & {
    return static_cast<int>(impl.cvcdu.vc_pdu._m_pdu_spare);
  }

  auto m_pdu_spare() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).m_pdu_spare(); },
      [this](int x) {
        impl.cvcdu.vc_pdu._m_pdu_spare = x;
        dirty_checksum = true;
      }
    };
  }

  auto first_header_pointer() const & {
    return static_cast<int>(impl.cvcdu.vc_pdu._first_header_pointer_h << 8 | impl.cvcdu.vc_pdu._first_header_pointer_l);
  }

  auto first_header_pointer() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).first_header_pointer(); },
      [this](int x) {
        impl.cvcdu.vc_pdu._first_header_pointer_h = x >> 8; impl.cvcdu.vc_pdu._first_header_pointer_l= x & 0xff;
        dirty_checksum = true;
      }
    };
  }

  auto data() const & -> auto const & {
    return impl.cvcdu.vc_pdu._data;
  }

  auto data() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).data(); },
      [this](std::span<std::byte, cadu::DATA_LEN> s) { 
        std::copy(s.begin(), s.end(), impl.cvcdu.vc_pdu._data.begin());
        dirty_checksum = true;
      }
    };
  }

  auto data_header_aligned() const & -> auto const {
    return std::views::counted(data().begin() + first_header_pointer(), cadu::DATA_LEN - (first_header_pointer()));
  }

  auto data_header_aligned() & {
    return Proxy {
      [this]() -> decltype(auto) { return std::as_const(*this).data_header_aligned(); },
      // TODO: replace with std::span?
      [this](std::span<std::byte> s) {
        std::copy_n(
          s.begin(),
          std::min(static_cast<int>(s.size()), cadu::DATA_LEN - first_header_pointer()),
          impl.cvcdu.vc_pdu._data.begin() + first_header_pointer()
        );
        dirty_checksum = true;
      }
    };
  }

  auto checksum() const & -> auto const & {
    return impl.cvcdu._checksum;
  }

  auto checksum() & {
    return Proxy{
      [this]() -> decltype(auto) { return std::as_const(*this).checksum(); },
      [this](std::array<std::byte, 128> x) { impl.cvcdu._checksum = x; }
    };
  }

  // TODO: place a lock on dirty_checksum and the checksum itself, as this isn't thread safe
  // const methods are assumed thread safe, which this isn't, because the checksum and dirty
  // bit are both mutable
  // This method is only const because the stream insertion operator needed to be
  // in order to use it 
  void recalculate_checksum() const {
    calculate_checksum(impl.cvcdu._checksum);
    dirty_checksum = false;
  }

  // TODO: replace with method which attempts to calculate where the errors are
  // TODO: implement method to correct bit errors given the checksum
  auto _validate_checksum() -> bool {
    if (dirty_checksum) {
      return false;
    } else {
      // Validate checksum
      auto buffer = std::array<std::byte, 128> {};
      calculate_checksum(buffer);
      return buffer == impl.cvcdu._checksum;
    }
  }

  friend auto nonrandomised::operator<<(std::ostream & output, ::CADU const & cadu) -> std::ostream &;
  friend auto nonrandomised::operator>>(std::istream & input, ::CADU & cadu) -> std::istream &;
  friend auto randomised::operator<<(std::ostream & output, ::CADU const & cadu) -> std::ostream &;
  friend auto randomised::operator>>(std::istream & input, ::CADU & cadu) -> std::istream &;

};

namespace nonrandomised {
  struct _CADU: ::CADU {
    _CADU () = default;
    _CADU (::CADU cadu) : ::CADU{cadu} {};
  };

  auto operator<<(std::ostream & output, ::CADU const & cadu) -> std::ostream & {
    if (cadu.dirty_checksum) {
      std::cerr << "dirty checksum. recalculating..." << std::endl;
      cadu.recalculate_checksum();
    }
    output.write(reinterpret_cast<const char*>(&cadu.impl), sizeof(cadu.impl));
    return output;
  }

  auto operator>>(std::istream & input, ::CADU & cadu) -> std::istream & {
    uint32_t prefix_buffer = 0;
    bool found_header = false;

    for (uint8_t byte_buffer; input >> byte_buffer;) {
      // Update prefix buffer
      prefix_buffer <<= 8;
      prefix_buffer |= byte_buffer;

      // Check for matching prefix
      if (prefix_buffer == cadu::SYNC_MARKER) {
        found_header = true;
        break;
      }
    }

    if (found_header) {
      // We found the next frame
      // TODO: bytes on the stack instead
      auto bytes = std::make_unique_for_overwrite<char[]>(sizeof(CVCDU)/sizeof(char));
      input.read(bytes.get(), sizeof(CVCDU));
      std::memcpy(&cadu.impl.cvcdu, bytes.get(), sizeof(CVCDU));
    }
    return input;
  }  
}

namespace randomised {
  struct _CADU: ::CADU {
    _CADU () = default;
    _CADU (::CADU cadu) : ::CADU{cadu} {};
  };

  auto operator<<(std::ostream & output, ::CADU const & cadu) -> std::ostream & {
    auto randomised_cadu = ::CADU(cadu);
    randomised_cadu.randomise();
    nonrandomised::operator<<(output, randomised_cadu);
    return output;
  }

  auto operator>>(std::istream & input, ::CADU & cadu) -> std::istream & {
    nonrandomised::operator>>(input, cadu);
    cadu.randomise();
    return input;
  }
}


// Add two new types in the (non)randomised namespaces:
  // CADU_randomised
  // nonrandomisedCADU
  // Inherit from the normal CADU, behaving the same except they have only one stream extraction
  // Add implicit conversion?
