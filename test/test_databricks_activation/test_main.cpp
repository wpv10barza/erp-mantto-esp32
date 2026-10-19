#include <unity.h>
#include "databricks_activation.h"
#include "home_menu.h"
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}
using databricks_activation::Activation;
using databricks_activation::State;
struct Mock {
  bool auth = true, health = false;
  int code = 200, posts = 0, polls = 0;
  bool authenticate() { return auth; }
  int start() { ++posts; return code; }
  bool healthy() { ++polls; return health; }
};
void test_start_is_explicit_and_requires_health() {
  Activation a; Mock t;
  a.tick(t, 6000); TEST_ASSERT_EQUAL(0,t.posts);
  a.begin(t,0); TEST_ASSERT_TRUE(a.state == State::Starting);
  a.begin(t,1); TEST_ASSERT_EQUAL(1,t.posts);
  a.tick(t,4999); TEST_ASSERT_EQUAL(0,t.polls);
  a.tick(t,5000); TEST_ASSERT_TRUE(a.state == State::Starting);
  t.health=true; a.tick(t,10000); TEST_ASSERT_TRUE(a.state == State::Ready);
  TEST_ASSERT_EQUAL(1,t.posts);
}
void test_failures_never_retry_start_or_report_ready() {
  for (int code : {-1,401,403,404,429,500}) {
    Activation a; Mock t; t.code=code; t.health=true;
    a.begin(t,0); a.tick(t,10000);
    TEST_ASSERT_FALSE(a.state == State::Ready);
    TEST_ASSERT_EQUAL(1,t.posts); TEST_ASSERT_EQUAL(0,t.polls);
  }
  Activation a; Mock t; t.auth=false;
  a.begin(t,0); TEST_ASSERT_EQUAL(0,t.posts); TEST_ASSERT_TRUE(a.state==State::AuthError);
}
void test_readiness_timeout_and_clock_wrap() {
  Activation a; Mock t; const uint32_t start=0xfffffff0U;
  a.begin(t,start); a.tick(t,start+5000U); TEST_ASSERT_EQUAL(1,t.polls);
  a.tick(t,start+180000U); TEST_ASSERT_TRUE(a.state==State::Unknown);
  TEST_ASSERT_EQUAL(1,t.posts);
}
void test_touch_start_region_is_separate_from_navigation() {
  TEST_ASSERT_TRUE(home_ui::databricksStartHit(240,380));
  TEST_ASSERT_FALSE(home_ui::databricksStartHit(240,320));
  TEST_ASSERT_FALSE(home_ui::databricksStartHit(240,440));
  TEST_ASSERT_TRUE(home_ui::hitTest(240,320,false)==home_ui::Action::Databricks);
  TEST_ASSERT_TRUE(home_ui::hitTest(240,320,true)==home_ui::Action::None);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_start_is_explicit_and_requires_health);
  RUN_TEST(test_failures_never_retry_start_or_report_ready);
  RUN_TEST(test_readiness_timeout_and_clock_wrap);
  RUN_TEST(test_touch_start_region_is_separate_from_navigation);
  return UNITY_END();
}
