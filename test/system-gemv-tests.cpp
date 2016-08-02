#include "simit-test.h"

#include "init.h"
#include "graph.h"
#include "tensor.h"
#include "program.h"
#include "error.h"

using namespace std;
using namespace simit;

TEST(System, gemv) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Taint c
  c.set(p0, 42.0);
  c.set(p2, 42.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p0));
  ASSERT_EQ(2.0, b.get(p1));
  ASSERT_EQ(3.0, b.get(p2));

  // Check that outputs are correct
  ASSERT_EQ(3.0, c.get(p0));
  ASSERT_EQ(13.0, c.get(p1));
  ASSERT_EQ(10.0, c.get(p2));
}

TEST(System, gemv_stencil) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  // Springs
  Set springs(points,{3});
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  // Build points
  ElementRef p0 = springs.getLatticePoint({0});
  ElementRef p1 = springs.getLatticePoint({1});
  ElementRef p2 = springs.getLatticePoint({2});

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Taint c
  c.set(p0, 42.0);
  c.set(p2, 42.0);


  // Build springs
  ElementRef s0 = springs.getLatticeLink({0},0);
  ElementRef s1 = springs.getLatticeLink({1},0);
  ElementRef s2 = springs.getLatticeLink({2},0);

  a.set(s0, 1.0);
  a.set(s1, 2.0);
  a.set(s2, 0.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p0));
  ASSERT_EQ(2.0, b.get(p1));
  ASSERT_EQ(3.0, b.get(p2));

  // Check that outputs are correct
  ASSERT_EQ(3.0, c.get(p0));
  ASSERT_EQ(13.0, c.get(p1));
  ASSERT_EQ(10.0, c.get(p2));
}

TEST(System, gemv_stencil_indexless) {
  // HACK: Set kIndexlessStencils to true for this type of test
  kIndexlessStencils = true;
  
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  // Springs
  Set springs(points,{3});
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  // Build points
  ElementRef p0 = springs.getLatticePoint({0});
  ElementRef p1 = springs.getLatticePoint({1});
  ElementRef p2 = springs.getLatticePoint({2});

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Taint c
  c.set(p0, 42.0);
  c.set(p2, 42.0);


  // Build springs
  ElementRef s0 = springs.getLatticeLink({0},0);
  ElementRef s1 = springs.getLatticeLink({1},0);
  ElementRef s2 = springs.getLatticeLink({2},0);

  a.set(s0, 1.0);
  a.set(s1, 2.0);
  a.set(s2, 0.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p0));
  ASSERT_EQ(2.0, b.get(p1));
  ASSERT_EQ(3.0, b.get(p2));

  // Check that outputs are correct
  ASSERT_EQ(3.0, c.get(p0));
  ASSERT_EQ(13.0, c.get(p1));
  ASSERT_EQ(10.0, c.get(p2));

  kIndexlessStencils = false;
}

TEST(System, gemv_stencil_2d) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  // Springs
  Set springs(points,{3,2}); // rectangular lattice
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  // Build points
  ElementRef p00 = springs.getLatticePoint({0,0});
  ElementRef p01 = springs.getLatticePoint({1,0});
  ElementRef p02 = springs.getLatticePoint({2,0});
  ElementRef p10 = springs.getLatticePoint({0,1});
  ElementRef p11 = springs.getLatticePoint({1,1});
  ElementRef p12 = springs.getLatticePoint({2,1});

  b.set(p00, 1.0);
  b.set(p01, 2.0);
  b.set(p02, 3.0);
  b.set(p10, 4.0);
  b.set(p11, 5.0);
  b.set(p12, 6.0);

  // Taint c
  c.set(p00, 42.0);
  c.set(p12, 42.0);


  // Build springs
  ElementRef s000 = springs.getLatticeLink({0,0},0);
  ElementRef s001 = springs.getLatticeLink({1,0},0);
  ElementRef s002 = springs.getLatticeLink({2,0},0);
  ElementRef s010 = springs.getLatticeLink({0,1},0);
  ElementRef s011 = springs.getLatticeLink({1,1},0);
  ElementRef s012 = springs.getLatticeLink({2,1},0);
  ElementRef s100 = springs.getLatticeLink({0,0},1);
  ElementRef s101 = springs.getLatticeLink({1,0},1);
  ElementRef s102 = springs.getLatticeLink({2,0},1);
  ElementRef s110 = springs.getLatticeLink({0,1},1);
  ElementRef s111 = springs.getLatticeLink({1,1},1);
  ElementRef s112 = springs.getLatticeLink({2,1},1);

  a.set(s000, 1.0);
  a.set(s001, 2.0);
  a.set(s002, 3.0);
  a.set(s010, 4.0);
  a.set(s011, 5.0);
  a.set(s012, 6.0);
  a.set(s100, 7.0);
  a.set(s101, 8.0);
  a.set(s102, 9.0);
  a.set(s110, 10.0);
  a.set(s111, 11.0);
  a.set(s112, 12.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p00));
  ASSERT_EQ(2.0, b.get(p01));
  ASSERT_EQ(3.0, b.get(p02));
  ASSERT_EQ(4.0, b.get(p10));
  ASSERT_EQ(5.0, b.get(p11));
  ASSERT_EQ(6.0, b.get(p12));

  // Check that outputs are correct
  ASSERT_EQ(100.0, (simit_float)c.get(p00));
  ASSERT_EQ(146.0, (simit_float)c.get(p01));
  ASSERT_EQ(211.0, (simit_float)c.get(p02));
  ASSERT_EQ(181.0, (simit_float)c.get(p10));
  ASSERT_EQ(224.0, (simit_float)c.get(p11));
  ASSERT_EQ(304.0, (simit_float)c.get(p12));
}

TEST(System, gemv_stencil_2d_indexless) {
  // HACK: Set kIndexlessStencils to true for this type of test
  kIndexlessStencils = true;

  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  // Springs
  Set springs(points,{3,2}); // rectangular lattice
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  // Build points
  ElementRef p00 = springs.getLatticePoint({0,0});
  ElementRef p01 = springs.getLatticePoint({1,0});
  ElementRef p02 = springs.getLatticePoint({2,0});
  ElementRef p10 = springs.getLatticePoint({0,1});
  ElementRef p11 = springs.getLatticePoint({1,1});
  ElementRef p12 = springs.getLatticePoint({2,1});

  b.set(p00, 1.0);
  b.set(p01, 2.0);
  b.set(p02, 3.0);
  b.set(p10, 4.0);
  b.set(p11, 5.0);
  b.set(p12, 6.0);

  // Taint c
  c.set(p00, 42.0);
  c.set(p12, 42.0);


  // Build springs
  ElementRef s000 = springs.getLatticeLink({0,0},0);
  ElementRef s001 = springs.getLatticeLink({1,0},0);
  ElementRef s002 = springs.getLatticeLink({2,0},0);
  ElementRef s010 = springs.getLatticeLink({0,1},0);
  ElementRef s011 = springs.getLatticeLink({1,1},0);
  ElementRef s012 = springs.getLatticeLink({2,1},0);
  ElementRef s100 = springs.getLatticeLink({0,0},1);
  ElementRef s101 = springs.getLatticeLink({1,0},1);
  ElementRef s102 = springs.getLatticeLink({2,0},1);
  ElementRef s110 = springs.getLatticeLink({0,1},1);
  ElementRef s111 = springs.getLatticeLink({1,1},1);
  ElementRef s112 = springs.getLatticeLink({2,1},1);

  a.set(s000, 1.0);
  a.set(s001, 2.0);
  a.set(s002, 3.0);
  a.set(s010, 4.0);
  a.set(s011, 5.0);
  a.set(s012, 6.0);
  a.set(s100, 7.0);
  a.set(s101, 8.0);
  a.set(s102, 9.0);
  a.set(s110, 10.0);
  a.set(s111, 11.0);
  a.set(s112, 12.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p00));
  ASSERT_EQ(2.0, b.get(p01));
  ASSERT_EQ(3.0, b.get(p02));
  ASSERT_EQ(4.0, b.get(p10));
  ASSERT_EQ(5.0, b.get(p11));
  ASSERT_EQ(6.0, b.get(p12));

  // Check that outputs are correct
  ASSERT_EQ(100.0, (simit_float)c.get(p00));
  ASSERT_EQ(146.0, (simit_float)c.get(p01));
  ASSERT_EQ(211.0, (simit_float)c.get(p02));
  ASSERT_EQ(181.0, (simit_float)c.get(p10));
  ASSERT_EQ(224.0, (simit_float)c.get(p11));
  ASSERT_EQ(304.0, (simit_float)c.get(p12));

  kIndexlessStencils = false;
}

TEST(System, gemv_add) {
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Taint c
  c.set(p0, 42.0);
  c.set(p2, 42.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p0));
  ASSERT_EQ(2.0, b.get(p1));
  ASSERT_EQ(3.0, b.get(p2));

  // Check that outputs are correct
  ASSERT_EQ(6.0, c.get(p0));
  ASSERT_EQ(26.0, c.get(p1));
  ASSERT_EQ(20.0, c.get(p2));
}


TEST(System, gemv_storage) {
  // This test tests whether we determine storage correctly for matrices
  // that do not come (directly) from maps
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Taint c
  c.set(p0, 42.0);
  c.set(p2, 42.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p0));
  ASSERT_EQ(2.0, b.get(p1));
  ASSERT_EQ(3.0, b.get(p2));

  // Check that outputs are correct
  ASSERT_EQ(6.0, c.get(p0));
  ASSERT_EQ(26.0, c.get(p1));
  ASSERT_EQ(20.0, c.get(p2));
}


TEST(System, gemv_diagonal) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(1.0, c.get(p0));
  ASSERT_EQ(6.0, c.get(p1));
  ASSERT_EQ(6.0, c.get(p2));
}

TEST(System, DISABLED_gemv_diagonal_func) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(1.0, c.get(p0));
  ASSERT_EQ(6.0, c.get(p1));
  ASSERT_EQ(6.0, c.get(p2));
}

TEST(System, gemv_diagonal_extraparams) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(2.0, c.get(p0));
  ASSERT_EQ(12.0, c.get(p1));
  ASSERT_EQ(12.0, c.get(p2));
}

TEST(System, gemv_diagonal_inout) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(2.0, a.get(s0));
  ASSERT_EQ(4.0, a.get(s1));

  ASSERT_EQ(2.0, c.get(p0));
  ASSERT_EQ(12.0, c.get(p1));
  ASSERT_EQ(12.0, c.get(p2));
}

TEST(System, gemv_generics) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Taint c
  c.set(p0, 42.0);
  c.set(p2, 42.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p0));
  ASSERT_EQ(2.0, b.get(p1));
  ASSERT_EQ(3.0, b.get(p2));

  // Check that outputs are correct
  ASSERT_EQ(3.0, c.get(p0));
  ASSERT_EQ(13.0, c.get(p1));
  ASSERT_EQ(10.0, c.get(p2));
}

TEST(System, gemv_generics_hidden) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Taint c
  c.set(p0, 42.0);
  c.set(p2, 42.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p0));
  ASSERT_EQ(2.0, b.get(p1));
  ASSERT_EQ(3.0, b.get(p2));

  // Check that outputs are correct
  ASSERT_EQ(3.0, c.get(p0));
  ASSERT_EQ(13.0, c.get(p1));
  ASSERT_EQ(10.0, c.get(p2));
}

TEST(System, gemv_nw) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(1.0, c.get(p0));
  ASSERT_EQ(4.0, c.get(p1));
  ASSERT_EQ(0.0, c.get(p2));
}

TEST(System, gemv_sw) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(0.0, c.get(p0));
  ASSERT_EQ(1.0, c.get(p1));
  ASSERT_EQ(4.0, c.get(p2));
}

TEST(System, gemv_assemble_from_points) {
  // Points
  Set points;
  FieldRef<simit_float> a = points.addField<simit_float>("a");
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  a.set(p0, 1.0);
  a.set(p1, 2.0);
  a.set(p2, 3.0);

  c.set(p0, 42.0);

  // Springs
  Set springs(points,points);

  springs.add(p0,p1);
  springs.add(p1,p2);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(1.0, c.get(p0));
  ASSERT_EQ(4.0, c.get(p1));
  ASSERT_EQ(0.0, c.get(p2));
}

TEST(System, gemv_inplace) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(3.0,  (double)b.get(p0));
  ASSERT_EQ(13.0, (double)b.get(p1));
  ASSERT_EQ(10.0, (double)b.get(p2));
}

TEST(System, gemv_blocked) {
  // Points
  Set points;
  FieldRef<simit_float,2> b = points.addField<simit_float,2>("b");
  FieldRef<simit_float,2> c = points.addField<simit_float,2>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, {1.0, 2.0});
  b.set(p1, {3.0, 4.0});
  b.set(p2, {5.0, 6.0});

  // Taint c
  c.set(p0, {42.0, 42.0});
  c.set(p2, {42.0, 42.0});

  // Springs
  Set springs(points,points);
  FieldRef<simit_float,2,2> a = springs.addField<simit_float,2,2>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, {1.0, 2.0, 3.0, 4.0});
  a.set(s1, {5.0, 6.0, 7.0, 8.0});

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  // TODO: add support for comparing a tensorref like so: b0 == {1.0, 2.0, 3.0}
  TensorRef<simit_float,2> c0 = c.get(p0);
  ASSERT_EQ(16.0, c0(0));
  ASSERT_EQ(36.0, c0(1));

  TensorRef<simit_float,2> c1 = c.get(p1);
  ASSERT_EQ(116.0, c1(0));
  ASSERT_EQ(172.0, c1(1));

  TensorRef<simit_float,2> c2 = c.get(p2);
  ASSERT_EQ(100.0, c2(0));
  ASSERT_EQ(136.0, c2(1));
}

TEST(System, gemv_blocked_nw) {
  // Points
  Set points;
  FieldRef<simit_float,2> b = points.addField<simit_float,2>("b");
  FieldRef<simit_float,2> c = points.addField<simit_float,2>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, {1.0, 2.0});
  b.set(p1, {3.0, 4.0});
  b.set(p2, {5.0, 6.0});

  // Taint c
  c.set(p0, {42.0, 42.0});
  c.set(p2, {42.0, 42.0});

  // Springs
  Set springs(points,points);
  FieldRef<simit_float,2,2> a = springs.addField<simit_float,2,2>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, {1.0, 2.0, 3.0, 4.0});
  a.set(s1, {5.0, 6.0, 7.0, 8.0});

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  // TODO: add support for comparing a tensorref like so: b0 == {1.0, 2.0, 3.0}
  TensorRef<simit_float,2> c0 = c.get(p0);
  ASSERT_EQ(5.0, c0(0));
  ASSERT_EQ(11.0, c0(1));

  TensorRef<simit_float,2> c1 = c.get(p1);
  ASSERT_EQ(39.0, c1(0));
  ASSERT_EQ(53.0, c1(1));

  TensorRef<simit_float,2> c2 = c.get(p2);
  ASSERT_EQ(0.0, c2(0));
  ASSERT_EQ(0.0, c2(1));
}

TEST(System, gemv_blocked_computed) {
  // Points
  Set points;
  FieldRef<simit_float,2> b = points.addField<simit_float,2>("b");
  FieldRef<simit_float,2> c = points.addField<simit_float,2>("c");
  FieldRef<simit_float,2> x = points.addField<simit_float,2>("x");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, {1.0, 2.0});
  b.set(p1, {3.0, 4.0});
  b.set(p2, {5.0, 6.0});

  x.set(p0, {1.0, 2.0});
  x.set(p1, {3.0, 4.0});
  x.set(p2, {5.0, 6.0});

  // Taint c
  c.set(p0, {42.0, 42.0});
  c.set(p2, {42.0, 42.0});

  // Springs
  Set springs(points,points);
  springs.add(p0,p1);
  springs.add(p1,p2);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  // TODO: add support for comparing a tensorref like so: b0 == {1.0, 2.0, 3.0}
  TensorRef<simit_float,2> c0 = c.get(p0);
  ASSERT_EQ(36.0, c0(0));
  ASSERT_EQ(72.0, c0(1));

  TensorRef<simit_float,2> c1 = c.get(p1);
  ASSERT_EQ(336.0, c1(0));
  ASSERT_EQ(472.0, c1(1));

  TensorRef<simit_float,2> c2 = c.get(p2);
  ASSERT_EQ(300.0, c2(0));
  ASSERT_EQ(400.0, c2(1));
}

TEST(System, gemv_input) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Taint c
  c.set(p0, 42.0);
  c.set(p2, 42.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  simit::Tensor<simit_float, 2> cs;
  cs(0) = 1.0;
  cs(1) = 2.0;

  func.bind("c", &cs);

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p0));
  ASSERT_EQ(2.0, b.get(p1));
  ASSERT_EQ(3.0, b.get(p2));

  // Check that outputs are correct
  ASSERT_EQ(6.0, (simit_float)c.get(p0));
  ASSERT_EQ(26.0, (simit_float)c.get(p1));
  ASSERT_EQ(20.0, (simit_float)c.get(p2));
}

TEST(System, gemv_diagonal_storage) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");
  FieldRef<simit_float> a = points.addField<simit_float>("a");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  a.set(p0, 1.0);
  a.set(p1, 3.0);
  a.set(p2, 2.0);

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Springs
  Set springs(points,points);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(1.0, c.get(p0));
  ASSERT_EQ(6.0, c.get(p1));
  ASSERT_EQ(6.0, c.get(p2));
}

TEST(System, gemv_blocked_diagonal_storage) {
  Set points;
  FieldRef<simit_float,2> b = points.addField<simit_float,2>("b");
  FieldRef<simit_float,2> c = points.addField<simit_float,2>("c");
  FieldRef<simit_float,2> a = points.addField<simit_float,2>("a");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  a.set(p0, {1.0, 2.0});
  a.set(p1, {3.0, 4.0});
  a.set(p2, {5.0, 6.0});

  b.set(p0, {1.0, 2.0});
  b.set(p1, {3.0, 4.0});
  b.set(p2, {5.0, 6.0});

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(5.0,   c.get(p0)(0));
  ASSERT_EQ(10.0,  c.get(p0)(1));

  ASSERT_EQ(75.0,  c.get(p1)(0));
  ASSERT_EQ(100.0, c.get(p1)(1));

  ASSERT_EQ(305.0, c.get(p2)(0));
  ASSERT_EQ(366.0, c.get(p2)(1));
}

TEST(System, gemv_blocked_scaled_diagonal) {
  Set points;
  FieldRef<simit_float,2> b = points.addField<simit_float,2>("b");
  FieldRef<simit_float,2> c = points.addField<simit_float,2>("c");
  FieldRef<simit_float,2> a = points.addField<simit_float,2>("a");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  a.set(p0, {1.0, 2.0});
  a.set(p1, {3.0, 4.0});
  a.set(p2, {5.0, 6.0});

  b.set(p0, {1.0, 1.0});
  b.set(p1, {1.0, 1.0});
  b.set(p2, {1.0, 1.0});

  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();
  func.bind("points", &points);
  func.runSafe();

  ASSERT_EQ(30.0,  c.get(p0)(0));
  ASSERT_EQ(60.0,  c.get(p0)(1));

  ASSERT_EQ(210.0, c.get(p1)(0));
  ASSERT_EQ(280.0, c.get(p1)(1));

  ASSERT_EQ(550.0, c.get(p2)(0));
  ASSERT_EQ(660.0, c.get(p2)(1));
}

TEST(System, gemv_diagonal_storage_and_sysreduced) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");
  FieldRef<simit_float> a = points.addField<simit_float>("a");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  a.set(p0, 1.0);
  a.set(p1, 3.0);
  a.set(p2, 2.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> d = springs.addField<simit_float>("d");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  d.set(s0, 1.0);
  d.set(s1, 2.0);


  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that outputs are correct
  ASSERT_EQ(2.0, (simit_float)c.get(p0));
 }

 
TEST(System, DISABLED_gemv_pass_element) {
  // Points
  Set points;
  FieldRef<simit_float> b = points.addField<simit_float>("b");
  FieldRef<simit_float> c = points.addField<simit_float>("c");

  ElementRef p0 = points.add();
  ElementRef p1 = points.add();
  ElementRef p2 = points.add();

  b.set(p0, 1.0);
  b.set(p1, 2.0);
  b.set(p2, 3.0);

  // Taint c
  c.set(p0, 42.0);
  c.set(p2, 42.0);

  // Springs
  Set springs(points,points);
  FieldRef<simit_float> a = springs.addField<simit_float>("a");

  ElementRef s0 = springs.add(p0,p1);
  ElementRef s1 = springs.add(p1,p2);

  a.set(s0, 1.0);
  a.set(s1, 2.0);

  // Compile program and bind arguments
  Function func = loadFunction(TEST_FILE_NAME, "main");
  if (!func.defined()) FAIL();

  func.bind("points", &points);
  func.bind("springs", &springs);

  func.runSafe();

  // Check that inputs are preserved
  ASSERT_EQ(1.0, b.get(p0));
  ASSERT_EQ(2.0, b.get(p1));
  ASSERT_EQ(3.0, b.get(p2));

  // Check that outputs are correct
  ASSERT_EQ(3.0, c.get(p0));
  ASSERT_EQ(13.0, c.get(p1));
  ASSERT_EQ(10.0, c.get(p2));
}

