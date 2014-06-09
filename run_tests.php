<?php

// This is a simple test jig for testing JsonPatch.inc against
// the tests in json-patch-tests.

require_once('JsonPatch.inc');

function print_test($test)
{
  print "{ ";
  $first = true;
  foreach(array('comment', 'doc', 'patch', 'expected', 'error') as $key)
  {
    if (array_key_exists($key, $test))
    {
      if (!$first)
      {
        print ",\n  ";
      }
      $first = false;
      print("\"$key\": " . json_encode($test[$key]));
    }
  }
  print " }\n";
}


// 'Recursive ksort' - prepare a php array s.t. json_encode might produce
// a canonical string.
function rksort($array = null) {
  if ($array === null || !is_array($array))
  {
    return $array;
  }
  ksort($array);
  foreach (array_keys($array) as $key) {
    $array[$key] = rksort($array[$key]);
  }
  return $array;
}


function do_test($test, $simplexml_mode=false) {
  // Allow 'comment-only' test records
  if (!(isset($test['doc']) && isset($test['patch'])))
     return true;
  try {
    $patched =  JsonPatch::patch($test['doc'], $test['patch'], $simplexml_mode);

    if (isset($test['error'])) {
      print("test failed: expected error didn't occur\n");
      print_test($test);
      print("found: ");
      print json_encode($patched);
      print("\n\n");
    }

    if (!isset($test['expected'])) {
      return true;
    }

    // XXX positive test here re: if error happens, and no exception, fail!

    if (is_array($patched)) $patched = rksort($patched);
    if (is_array($test['expected'])) $test['expected'] = rksort($test['expected']);
    if (json_encode($patched) !== json_encode($test['expected'])) {
      print("test failed:\n");
      print_test($test);
      print("found: " . json_encode($patched) . "\n\n");
      return false;
    } else {
      return true;
    }
  } catch (Exception $ex) {
    if (!isset($test['error'])) {
      print("test failed with exception: " . $ex->getMessage() . "\n");
      print_test($test);
      return false;
    } else {
      print("caught expected error: " . $ex->getMessage() . "\n");
      print("expected: " . $test['error'] . "\n\n");
      return true;
    }
  }
}      


// Piggyback on patch tests to test diff as well - use 'doc' and
// 'expected' from testcases.  Generate a diff, apply it, and check
// that it matches the target - in both directions.
function do_diff_test($test) {
  // Skip comment-only or test op tests
  if (!(isset($test['doc']) && isset($test['expected'])))
     return true;

  try {
    $doc1 = $test['doc']; // copy, in case sort/patch alters
    $doc2 = $test['expected'];
    $patch = JsonPatch::diff($doc1, $doc2);
    $patched = JsonPatch::patch($doc1, $patch);
    if (is_array($patched)) $patched = rksort($patched);
    if (is_array($doc2)) $doc2 = rksort($doc2);
    if (json_encode($patched) !== json_encode($doc2)) {
      print("diff test failed:\n");
      print("diff:     " . json_encode($patch) . "\n");
      print("found:    " . json_encode($patched) . "\n");
      print("expected: " . json_encode($doc2) . "\n\n");

      return false;
    }
    
    // reverse order
    $doc1 = $test['expected']; // copy, in case sort/patch alters
    $doc2 = $test['doc'];
    $patch = JsonPatch::diff($doc1, $doc2);
    $patched = JsonPatch::patch($doc1, $patch);
    if (is_array($patched)) $patched = rksort($patched);
    if (is_array($doc2)) $doc2 = rksort($doc2);
    if (json_encode($patched) !== json_encode($doc2)) {
      print("reverse diff test failed:\n");
      print("diff:     " . json_encode($patch) . "\n");
      print("found:    " . json_encode($patched) . "\n");
      print("expected: " . json_encode($doc2) . "\n\n");
      return false;
    }
  } catch (Exception $ex) {
    print("caught exception ".$ex->getMessage()."\n");
    return false;
  }
}


function test_file($filename, $simplexml_mode=false)
{
  $testfile = file_get_contents($filename);
  if (!$testfile)
  {
    throw new Exception("Couldn't find test file $filename");
    return false;
  }

  $tests = json_decode($testfile, 1);
  if (is_null($tests)) {
    throw new Exception("Error json-decoding test file $filename");
  }

  $success = true;
  foreach ($tests as $test) {
    if (isset($test['disabled']))
      continue;
    if (!do_test($test))
    {
      $success = false;
    }
  }
  return $success;
}


function main()
{
  $result = true;
  $testfiles = array(
                     'local_tests.json',
                     'json-patch-tests/tests.json',
                     'json-patch-tests/spec_tests.json'
                     );
  foreach ($testfiles as $testfile)
  {
    if (!test_file($testfile))
    {
      $result = false;
    }
  }
  /* if (!test_file('simplexml_tests.json', true)) */
  /* { */
  /*   $result = false; */
  /* } */
  return $result;
}


if (!main())
{
  exit(1);
}
else
{
  exit(0);
}