#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <sqleger/db.hpp>
#include <sqleger/stmt.hpp>
#include <sqleger/value.hpp>


using namespace sqleger;
using namespace sqleger::string_span_literals;


TEST_CASE("A value holds an sqlite3_value*", "[value]")
{
  SECTION("default constructor")
  {
    value v;
    REQUIRE(v.c_ptr() == nullptr);
    REQUIRE(!v);
  }

  SECTION("pointer constructor")
  {
    auto d = db(":memory:");

    auto s1 = stmt(d, "CREATE TABLE t(x INTEGER)"_ss);
    REQUIRE(s1.step() == result_t::done);

    auto s2 = stmt(d, "INSERT INTO t VALUES(2)");
    REQUIRE(s2.step() == result_t::done);

    auto s3 = stmt(d, "SELECT x FROM t");

    const auto* const c_ptr1 = ::sqlite3_column_value(s3.c_ptr(), 0);
    REQUIRE(c_ptr1 != nullptr);

    auto* const c_ptr2 = ::sqlite3_value_dup(c_ptr1);

    value v {c_ptr2};
    REQUIRE(v.c_ptr() == c_ptr2);
    REQUIRE(v);

    auto* const c_ptr3 = v.take_c_ptr();

    REQUIRE(c_ptr3 == c_ptr2);
    REQUIRE(!v);

    ::sqlite3_value_free(c_ptr2);
  }
}

TEST_CASE("A value can be freed", "[value]")
{
  auto d = db(":memory:");

  auto s1 = stmt(d, "CREATE TABLE t(x INTEGER)"_ss);
  REQUIRE(s1.step() == result_t::done);

  auto s2 = stmt(d, "INSERT INTO t VALUES(2)");
  REQUIRE(s2.step() == result_t::done);

  auto s3 = stmt(d, "SELECT x FROM t");

  const auto* const c_ptr = ::sqlite3_column_value(s3.c_ptr(), 0);
  REQUIRE(c_ptr != nullptr);

  auto v = value(::sqlite3_value_dup(c_ptr));

  v.free();
  REQUIRE(!v);
}

TEST_CASE("A value ref can be constructed", "[value]")
{
  SECTION("from pointer")
  {
    auto d = db(":memory:");

    auto s1 = stmt(d, "CREATE TABLE t(x INTEGER)"_ss);
    REQUIRE(s1.step() == result_t::done);

    auto s2 = stmt(d, "INSERT INTO t VALUES(2)"_ss);
    REQUIRE(s2.step() == result_t::done);

    auto s3 = stmt(d, "SELECT x FROM t"_ss);

    auto* const c_ptr = ::sqlite3_column_value(s3.c_ptr(), 0);

    auto vr = value_ref(c_ptr);

    REQUIRE(vr.c_ptr() == c_ptr);
  }

  SECTION("from value")
  {
    auto d = db(":memory:");

    auto s1 = stmt(d, "CREATE TABLE t(x INTEGER)"_ss);
    REQUIRE(s1.step() == result_t::done);

    auto s2 = stmt(d, "INSERT INTO t VALUES(2)"_ss);
    REQUIRE(s2.step() == result_t::done);

    auto s3 = stmt(d, "SELECT x FROM t"_ss);

    auto v = value(::sqlite3_value_dup(::sqlite3_column_value(s3.c_ptr(), 0)));
    auto vr = value_ref(v);

    REQUIRE(vr.c_ptr() == v.c_ptr());
  }
}

TEST_CASE("A value can be dupped", "[value]")
{
  auto d = db(":memory:");

  auto s1 = stmt(d, "CREATE TABLE t(x INTEGER)"_ss);
  REQUIRE(s1.step() == result_t::done);

  auto s2 = stmt(d, "INSERT INTO t VALUES(2)"_ss);
  REQUIRE(s2.step() == result_t::done);

  auto s3 = stmt(d, "SELECT x FROM t"_ss);

  auto vr = value_ref(::sqlite3_column_value(s3.c_ptr(), 0));

  auto v = vr.dup();

  REQUIRE(v);
  REQUIRE(v.c_ptr() != vr.c_ptr());
}

TEST_CASE("Data can be retrieved from a value", "[value]")
{
  auto d = db(":memory:");

  auto s1 = stmt(d,
                 "CREATE TABLE t("
                 "a BLOB NOT NULL,"
                 "b REAL NOT NULL,"
                 "c INTEGER NOT NULL,"
                 "d INTEGER NOT NULL,"
                 "e INTEGER,"
                 "f TEXT NOT NULL)"_ss);

  REQUIRE(s1.step() == result_t::done);

  auto s2 = stmt(d, "INSERT INTO t VALUES(?1, ?2, ?3, ?4, ?5, ?6)"_ss);

  const std::vector<uint64_t> vector = {1, 2, 3, 4, 5};
  constexpr auto ss = " vqlflz.tlue VPNRE103-====++++"_ss;

  REQUIRE(s2.bind_blob(1, vector) == result_t::ok);
  REQUIRE(s2.bind_double(2, 0.25) == result_t::ok);
  REQUIRE(s2.bind_int(3, 2) == result_t::ok);
  REQUIRE(s2.bind_int64(4, 3) == result_t::ok);
  REQUIRE(s2.bind_null(5) == result_t::ok);
  REQUIRE(s2.bind_text(6, ss) == result_t::ok);
  REQUIRE(s2.step() == result_t::done);

  auto s3 = stmt(d, "SELECT a, b, c, d, e, f FROM t"_ss);

  const auto r1 = s3.step();
  REQUIRE(r1 == result_t::row);

  auto v0 = value_ref(::sqlite3_column_value(s3.c_ptr(), 0));
  auto v1 = value_ref(::sqlite3_column_value(s3.c_ptr(), 1));
  auto v2 = value_ref(::sqlite3_column_value(s3.c_ptr(), 2));
  auto v3 = value_ref(::sqlite3_column_value(s3.c_ptr(), 3));
  auto v4 = value_ref(::sqlite3_column_value(s3.c_ptr(), 4));
  auto v5 = value_ref(::sqlite3_column_value(s3.c_ptr(), 5));

  const auto dt0 = v0.type();
  REQUIRE(dt0 == datatype_t::blob);

  const auto dt1 = v1.type();
  REQUIRE(dt1 == datatype_t::_float);

  const auto dt2 = v2.type();
  REQUIRE(dt2 == datatype_t::integer);

  const auto dt3 = v3.type();
  REQUIRE(dt3 == datatype_t::integer);

  const auto dt4 = v4.type();
  REQUIRE(dt4 == datatype_t::null);

  const auto dt5 = v5.type();
  REQUIRE(dt5 == datatype_t::text);

  const auto c0_sz = v0.bytes();
  REQUIRE(c0_sz == 40);

  const auto* const c0 = v0.blob();
  const auto c0_vector = [&]() {
    auto vector = std::vector<uint64_t>(5);
    std::memcpy(reinterpret_cast<void*>(vector.data()), c0, c0_sz);
    return vector;
  }();
  REQUIRE(c0_vector == vector);

  const auto c1 = v1._double();
  REQUIRE(c1 == 0.25);

  const auto c2 = v2._int();
  REQUIRE(c2 == 2);

  const auto c3 = v3.int64();
  REQUIRE(c3 == int64(3));

  const auto c5_sz = v5.bytes();
  REQUIRE(c5_sz == ss.length());

  const auto c5 = v5.text();
  const auto c5_ascii = utf8_to_ascii(c5);
  REQUIRE(string_span(c5_ascii) == ss);

  const auto r2 = s3.step();
  REQUIRE(r2 == result_t::done);
}