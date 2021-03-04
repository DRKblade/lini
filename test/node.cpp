#include "test.h"

#include <fstream>

struct parse_test_single {
  string path, value, parsed;
  bool fail{false}, exception{false}, clone_fail{false};
};
using parse_test = vector<parse_test_single>;

void test_nodes(parse_test testset, int repeat = base_repeat * 100) {
  auto doc = new node::wrapper();
  auto base_doc = node::base_p(doc);

  node::parse_context context{doc, nullptr, nullptr, true};
  // Add keys to doc
  for(auto test : testset) {
    tstring ts{test.value};
    auto last_count = get_test_part_count();
    if (test.fail)
      EXPECT_ANY_THROW(doc->add(test.path, test.value, ts, context)) << "Expected error";
    else
      EXPECT_NO_THROW(doc->add(test.path, test.value, ts, context)) << "Unexpected error";
    if (last_count != get_test_part_count())
      cerr << "Key: " << test.path << endl;
  }
  auto test_doc = [&](node::base_p node, node::errorlist& errs) {
    auto doc = dynamic_cast<node::wrapper*>(node.get());
    for (auto test : testset) {
      // Skip key if it is expected to fail in the previous step
      if (test.fail || test.clone_fail) {
        std::erase_if(errs, [&](auto pair) { return pair.first == test.path; });
      } else {
        check_key(*doc, test.path, test.parsed, test.exception);
      }
    }
  };
  triple_node_test(base_doc, test_doc, repeat);
}

TEST(Node, Simple) {
  test_nodes({
    {".key", "foo", "foo"},
    {"a.ref", "${.key}", "foo"},
    {"a.ref-space", "${ .key }", "foo"},
  });
}

TEST(Node, Cmd) {
  test_nodes({
    {"msg", "1.000", "1.000"},
    {"cmd", "${cmd echo ${msg}}", "1.000"},
    {"cmd-ref", "${map 1 2 ${cmd}}", "2.000000"},
    {"cmd-msg", "result is ${cmd-ref}", "result is 2.000000"},
  }, base_repeat);
  test_nodes({{"cmd", "${cmd echo hello world}", "hello world"}}, base_repeat);
  test_nodes({{"cmd", "${cmd nexist}", "", false, true}}, base_repeat);
  test_nodes({{"cmd", "${cmd nexist ? fail}", "fail"}}, base_repeat);
}

TEST(Node, Ref) {
  test_nodes({
    {"test2.ref-fake", "{test.key-a}", "{test.key-a}"},
    {"test2.ref-file-default-before", "${file nexist.txt ? ${test3.ref-ref-a}}", "a"},
    {"test2.ref-before", "${test2.ref-a}", "a"},
    {"test.key-a", "a", "a"},
    {"test2.ref-a", "${test.key-a}", "a"},
    {"test3.ref-ref-a", "${test2.ref-a?failed}", "a"},
    {"test.ref-ref-a", "${test2.ref-a?failed}", "a"},
    {"test2.ref-default-a", "${test.key-nexist?${test.key-a}}", "a"},
    {"test2.ref-file-default", "${file nexist.txt ? ${test.key-a}}", "a"},
    {"test2.ref-nexist", "${test.key-nexist? \" f a i l ' }", "\" f a i l '"},
    {"test2.ref-fail", "${test.key-fail}", "${test.key-fail}", false, true, true},
    {"test2.interpolation", "This is ${test.key-a} test", "This is a test"},
    {"test2.interpolation2", "$ ${test.key-a}", "$ a"},
    {"test2.interpolation3", "} ${test.key-a}", "} a"},
    {"test2.escape", "\\${test.key-a}", "${test.key-a}"},
    {"test2.not-escape", "\\$${test.key-a}", "\\$a"},
  });
  test_nodes({
    {"ref-cyclic-1", "${.ref-cyclic-2}", "${ref-cyclic-1}", false, true, true},
    {"ref-cyclic-2", "${.ref-cyclic-1}", "${ref-cyclic-1}", false, true, true},
  });
  test_nodes({
    {"ref-cyclic-1", "${.ref-cyclic-2}", "${ref-cyclic-1}", false, true, true},
    {"ref-cyclic-2", "${.ref-cyclic-3}", "${ref-cyclic-1}", false, true, true},
    {"ref-cyclic-3", "${.ref-cyclic-1}", "${ref-cyclic-1}", false, true, true},
    {"ref-not-cyclic-1", "${ref-not-cyclic-2}", ""},
    {"ref-not-cyclic-2", "", ""}
  });
}
TEST(Node, File) {
  test_nodes({
    {"ext", "txt", "txt"},
    {"file", "${file key_file.${ext} ? fail}", "content"},
    {"file-fail", "${file nexist.${ext} ? Can't find ${ext} file}", "Can't find txt file"},
  });
  test_nodes({{"file1", "${file key_file.txt }", "content"}});
  test_nodes({{"file2", "${file key_file.txt?fail}", "content"}});
  test_nodes({{"file3", "${file nexist.txt ? ${file key_file.txt}}", "content"}});
  test_nodes({{"file4", "${file nexist.txt ? \" f a i l ' }", "\" f a i l '"}});
  test_nodes({{"file5", "${file nexist.txt}", "${file nexist.txt}", false, true}});
}

TEST(Node, Color) {
  test_nodes({
    {"color", "${color #123456 }", "#123456"},
    {"color-fallback", "${color nexist(1) ? #ffffff }", "#ffffff"},
    {"color-hsv", "${color hsv(180, 1, 0.75)}", "#00BFBF"},
    {"color-ref", "${color ${color}}", "#123456"},
    {"color-mod", "${color cielch 'lum * 1.5, hue + 60' ${color}}", "#633E5C"},
  });
}

TEST(Node, Clone) {
  test_nodes({
    {"clone_source", "${color #123456 }", "#123456"},
    {"clone_source.lv1", "abc", "abc"},
    {"clone_source.lv1.lv2", "abc", "abc"},
    {"clone_source.dumb", "${nexist}", "", false, true, true},
    {"clone", "${clone clone_source }", "#123456"},
    {"clone.lv1", "def", "def", true},
    {"clone.lv1.lv2", "def", "def", true},
    {"clone.lv1.dumb", "abc", "abc"},
    {"clone_merge", "${clone clone_source clone}", "#123456"},
    {"clone_merge.lv1.dumb", "def", "def", true},
  });
  test_nodes({{"clone2", "${clone nexist nexist2 }", "", true}});
  test_nodes({{"clone3", "${clone nexist}", "", true}});
}

TEST(Node, Other) {
  setenv("test_env", "test_env", true);
  unsetenv("nexist");

  test_nodes({{"interpolate", "%{${color hsv(0, 1, 0.5)}}", "%{#800000}"}});
  test_nodes({{"dumb", "${dumb nexist.txt}", "${dumb nexist.txt}", true}});
  test_nodes({{"dumb", "", ""}});
  test_nodes({{"dumb", "${}", "", true}});
  test_nodes({{"env", "${env test_env? fail}", "test_env"}});
  test_nodes({{"env", "${env nexist? \" f a i l \" }", " f a i l "}});
  test_nodes({{"env", "${env nexist test_env }", "", true}});
  test_nodes({{"map", "${map 5:10 0:2 7.5}", "1.000000"}});
  test_nodes({{"map", "${map 5:10 2 7.5 ? -1}", "1.000000"}});
  test_nodes({{"map", "${map 5:10 7.5}", "1.000000", true}});
  test_nodes({
    {"base", "${map 100 1 ${rel stat}}", "", false, true},
    {"clone.stat", "60", "60"},
    {"clone", "${clone base}", "0.600000"},
    {"clone.stat", "60", "60", true},
  });
}
