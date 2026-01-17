#pragma once

#include <string>
#include "hotel_reservation.pb.h"
#include "padding_utils.h"
#include <vector>
#include "compression_utils.h"
#include <iostream>

namespace microservice {
namespace models {

// Threshold for when to compress strings in toProto
static constexpr size_t kStringCompressThreshold = 256;

inline std::string maybe_compress(const std::string& s) {
	if (s.size() >= kStringCompressThreshold) {
		// std::cout << "compress " << s << std::endl;
		return microservice::compression::compress_data(s);
	}
	return s;
}

inline std::string maybe_decompress(const std::string& s) {
	// decompress_data returns original if not prefixed with COMPRESSED:
	if (s.size() >= 11 && s.rfind("COMPRESSED:", 0) == 0) {
		// std::cout << "decompress " << s << std::endl;
	}
	return microservice::compression::decompress_data(s);
}

struct AddressData {
	std::string street_number;
	std::string street_name;
	std::string city;
	std::string state;
	std::string country;
	std::string postal_code;
	double lat = 0.0;
	double lon = 0.0;
};

struct HotelProfileData {
	std::string id;
	std::string name;
	std::string phone_number;
	std::string description;
	AddressData address;
};

struct RoomTypeData {
	double bookable_rate = 0.0;
	std::string code;
	std::string room_description;
	double total_rate = 0.0;
	double total_rate_inclusive = 0.0;
};

struct RatePlanData {
	std::string hotel_id;
	std::string code;
	std::string in_date;
	std::string out_date;
	RoomTypeData room_type;
};

struct ReservationData {
	std::string hotel_id;
	std::string customer_name;
	std::string in_date;
	std::string out_date;
	long long number = 0;
};

// Additional request models (arriving data)
struct GetProfilesRequestData {
	std::vector<std::string> hotel_ids;
	std::string locale;
};

struct GetRatesRequestData {
	std::vector<std::string> hotel_ids;
	std::string in_date;
	std::string out_date;
};

struct UserRequestData {
	std::string username;
	std::string password;
};

using CheckUserRequestData = UserRequestData;

struct ReservationRequestData {
	std::string in_date;
	std::string out_date;
	std::string hotel_id;
	std::string customer_name;
	std::string username;
	std::string password;
	long long room_number = 0;
};

struct NearbyRequestData {
	double lat = 0.0;
	double lon = 0.0;
};

struct NearbyResponseData {
	std::vector<std::string> hotel_ids;
};

struct RecommendRequestData {
	double lat = 0.0;
	double lon = 0.0;
	std::string require;
	std::string locale;
};

struct RecommendResponseData {
	std::vector<HotelProfileData> hotels;
};

struct SearchRequestData {
	double lat = 0.0;
	double lon = 0.0;
};

struct SearchResponseData {
	std::vector<HotelProfileData> hotels;
};

inline void toProto(const AddressData& src, hotelreservation::Address& dst) {
	dst.set_street_number(maybe_compress(src.street_number));
	dst.set_street_name(maybe_compress(src.street_name));
	dst.set_city(maybe_compress(src.city));
	dst.set_state(maybe_compress(src.state));
	dst.set_country(maybe_compress(src.country));
	dst.set_postal_code(maybe_compress(src.postal_code));
	dst.set_lat(src.lat);
	dst.set_lon(src.lon);
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const HotelProfileData& src, hotelreservation::HotelProfile& dst) {
	dst.set_id(maybe_compress(src.id));
	dst.set_name(maybe_compress(src.name));
	dst.set_phone_number(maybe_compress(src.phone_number));
	dst.set_description(maybe_compress(src.description));
	toProto(src.address, *dst.mutable_address());
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const RoomTypeData& src, hotelreservation::RoomType& dst) {
	dst.set_bookable_rate(src.bookable_rate);
	dst.set_code(maybe_compress(src.code));
	dst.set_room_description(maybe_compress(src.room_description));
	dst.set_total_rate(src.total_rate);
	dst.set_total_rate_inclusive(src.total_rate_inclusive);
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const ReservationData& src, hotelreservation::Reservation& dst) {
	dst.set_hotel_id(maybe_compress(src.hotel_id));
	dst.set_customer_name(maybe_compress(src.customer_name));
	dst.set_in_date(maybe_compress(src.in_date));
	dst.set_out_date(maybe_compress(src.out_date));
	dst.set_number(src.number);
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const RatePlanData& src, hotelreservation::RatePlan& dst) {
	dst.set_hotel_id(maybe_compress(src.hotel_id));
	dst.set_code(maybe_compress(src.code));
	dst.set_in_date(maybe_compress(src.in_date));
	dst.set_out_date(maybe_compress(src.out_date));
	toProto(src.room_type, *dst.mutable_room_type());
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const NearbyResponseData& src, hotelreservation::NearbyResponse& dst) {
	for (const auto& id : src.hotel_ids) {
		dst.add_hotel_ids(maybe_compress(id));
	}
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const RecommendResponseData& src, hotelreservation::RecommendResponse& dst) {
	for (const auto& hp : src.hotels) {
		auto* out = dst.add_hotels();
		toProto(hp, *out);
	}
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const SearchResponseData& src, hotelreservation::SearchResponse& dst) {
	for (const auto& hp : src.hotels) {
		auto* out = dst.add_hotels();
		toProto(hp, *out);
	}
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

// Request: model -> proto helpers for outbound inter-service calls
inline void toProto(const NearbyRequestData& src, hotelreservation::NearbyRequest& dst) {
	dst.set_lat(src.lat);
	dst.set_lon(src.lon);
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const GetProfilesRequestData& src, hotelreservation::GetProfilesRequest& dst) {
	for (const auto& id : src.hotel_ids) dst.add_hotel_ids(maybe_compress(id));
	dst.set_locale(maybe_compress(src.locale));
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const GetRatesRequestData& src, hotelreservation::GetRatesRequest& dst) {
	for (const auto& id : src.hotel_ids) dst.add_hotel_ids(maybe_compress(id));
	dst.set_in_date(maybe_compress(src.in_date));
	dst.set_out_date(maybe_compress(src.out_date));
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void toProto(const CheckUserRequestData& src, hotelreservation::CheckUserRequest& dst) {
	dst.set_username(maybe_compress(src.username));
	dst.set_password(maybe_compress(src.password));
	*dst.mutable_padding() = microservice::utils::generate_person_padding();
}

inline void fromProto(const hotelreservation::Address& src, AddressData& dst) {
	dst.street_number = maybe_decompress(src.street_number());
	dst.street_name = maybe_decompress(src.street_name());
	dst.city = maybe_decompress(src.city());
	dst.state = maybe_decompress(src.state());
	dst.country = maybe_decompress(src.country());
	dst.postal_code = maybe_decompress(src.postal_code());
	dst.lat = src.lat();
	dst.lon = src.lon();
}

inline void fromProto(const hotelreservation::HotelProfile& src, HotelProfileData& dst) {
	dst.id = maybe_decompress(src.id());
	dst.name = maybe_decompress(src.name());
	dst.phone_number = maybe_decompress(src.phone_number());
	dst.description = maybe_decompress(src.description());
	fromProto(src.address(), dst.address);
}

inline void fromProto(const hotelreservation::RecommendRequest& src, RecommendRequestData& dst) {
	dst.lat = src.lat();
	dst.lon = src.lon();
	dst.require = maybe_decompress(src.require());
	dst.locale = maybe_decompress(src.locale());
}

inline void fromProto(const hotelreservation::SearchRequest& src, SearchRequestData& dst) {
	dst.lat = src.lat();
	dst.lon = src.lon();
}

inline void fromProto(const hotelreservation::NearbyRequest& src, NearbyRequestData& dst) {
	dst.lat = src.lat();
	dst.lon = src.lon();
}

inline void fromProto(const hotelreservation::GetProfilesRequest& src, GetProfilesRequestData& dst) {
	dst.locale = maybe_decompress(src.locale());
	dst.hotel_ids.clear();
	for (const auto& id : src.hotel_ids()) dst.hotel_ids.push_back(maybe_decompress(id));
}

inline void fromProto(const hotelreservation::GetRatesRequest& src, GetRatesRequestData& dst) {
	dst.in_date = maybe_decompress(src.in_date());
	dst.out_date = maybe_decompress(src.out_date());
	dst.hotel_ids.clear();
	for (const auto& id : src.hotel_ids()) dst.hotel_ids.push_back(maybe_decompress(id));
}

inline void fromProto(const hotelreservation::UserRequest& src, UserRequestData& dst) {
	dst.username = maybe_decompress(src.username());
	dst.password = maybe_decompress(src.password());
}

inline void fromProto(const hotelreservation::CheckUserRequest& src, CheckUserRequestData& dst) {
	dst.username = maybe_decompress(src.username());
	dst.password = maybe_decompress(src.password());
}

inline void fromProto(const hotelreservation::ReservationRequest& src, ReservationRequestData& dst) {
	dst.in_date = maybe_decompress(src.in_date());
	dst.out_date = maybe_decompress(src.out_date());
	dst.hotel_id = maybe_decompress(src.hotel_id());
	dst.customer_name = maybe_decompress(src.customer_name());
	dst.username = maybe_decompress(src.username());
	dst.password = maybe_decompress(src.password());
	dst.room_number = src.room_number();
}

} // namespace models
} // namespace microservice


