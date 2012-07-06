<?

// This is a simple test jig for testing JsonPatch.inc against
// the tests in json-patch-tests.

// Lots of cleanup needed!

require_once('JsonPatch.inc');

function json_format($json)
{
  $tab = "  ";
  $new_json = "";
  $indent_level = 0;
  $in_string = false;
  
  $json_obj = json_decode($json);
  
  if($json_obj === false)
    return false;
  
  $json = json_encode($json_obj);
  $len = strlen($json);
  
  for($c = 0; $c < $len; $c++)
  {
    $char = $json[$c];
    switch($char)
    {
    case '{':
    case '[':
      if(!$in_string)
      {
        $new_json .= $char . "\n" . str_repeat($tab, $indent_level+1);
        $indent_level++;
      }
      else
      {
        $new_json .= $char;
      }
      break;
    case '}':
    case ']':
      if(!$in_string)
      {
        $indent_level--;
        $new_json .= "\n" . str_repeat($tab, $indent_level) . $char;
      }
      else
      {
        $new_json .= $char;
      }
      break;
    case ',':
      if(!$in_string)
      {
        $new_json .= ",\n" . str_repeat($tab, $indent_level);
      }
      else
      {
        $new_json .= $char;
      }
      break;
    case ':':
      if(!$in_string)
      {
        $new_json .= ": ";
      }
      else
      {
        $new_json .= $char;
      }
      break;
    case '"':
      if($c > 0 && $json[$c-1] != '\\')
      {
        $in_string = !$in_string;
      }
    default:
      $new_json .= $char;
      break;                   
    }
  }
  
  return $new_json;
}


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


// Piggyback on patch tests to test diff as well -
// use 'doc' and 'expected' from testcases.
// Generate a diff, apply it, and check that it matches the target -
// in both directions.
function do_diff_test($test, $testindex) {
  // Allow 'comment-only' test records
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
      print("$testindex failed:\n");
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
      print("$testindex failed:\n");
      print("diff:     " . json_encode($patch) . "\n");
      print("found:    " . json_encode($patched) . "\n");
      print("expected: " . json_encode($doc2) . "\n\n");
      return false;
    }
  } catch (Exception $ex) {
    print("$testindex: caught exception ".$ex->getMessage()."\n");
    return false;
  }
}


function do_test($test, $testindex) {
  // Allow 'comment-only' test records
  if (!(isset($test['doc']) && isset($test['patch'])))
     return true;
  try {
    $patched =  JsonPatch::patch($test['doc'], $test['patch']);

    if (isset($test['error'])) {
      print("$testindex: failed: expected error didn't occur\n");
      print(json_format(json_encode($test)));
      print("\n");
      print("found: ");
      print json_encode($patched);
      print("\n");
    }

    if (!isset($test['expected'])) {
      return true;
    }

    // XXX positive test here re: if error happens, and no exception, fail!

    if (is_array($patched)) $patched = rksort($patched);
    if (is_array($test['expected'])) $test['expected'] = rksort($test['expected']);
    if (json_encode($patched) !== json_encode($test['expected'])) {
      print("$testindex: failed:\n");
      print(json_format(json_encode($test)));
      print("\n");
      print("found:    " . json_encode($patched) . "\n");
      print("expected: " . json_encode($test['expected']) . "\n\n");
      return false;
    } else {
      return true;
    }
  } catch (Exception $ex) {
    if (!isset($test['error'])) {
      print("$testindex: failed with exception: " . $ex->getMessage() . "\n\n");
      return false;
    } else {
/*       print("$testindex: caught expected error: " . $ex->getMessage() . "\n"); */
/*       print("expected: " . $test['error'] . "\n\n"); */
      return true;
    }
  }
}      

$tests = json_decode(file_get_contents("..//json-patch-tests/tests.json"), 1);
if (is_null($tests)) {
   fatal("problem finding/decoding test file\n");
}
$i = 0;
foreach ($tests as $test) {
/*   print("$i\n"); */
  if (isset($test['disabled']))
    continue;

  do_test($test, $i);
  do_diff_test($test, $i);
  $i++;
}

print("\n");
