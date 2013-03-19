<?php

/*
Produce and apply json-patch objects.

Implements the IETF json-patch and json-pointer drafts:

http://tools.ietf.org/html/draft-ietf-appsawg-json-patch-02
http://tools.ietf.org/html/draft-ietf-appsawg-json-pointer-02

Entry points
------------

- get($doc, $pointer) - get a value from a json document
- diff($src, $dst) - return patches to create $dst from $src
- patch($doc, $patches) - apply patches to $doc and return result

Arguments are PHP arrays, i.e. the output of
json_decode($json_string, 1).
JSON strings may also be used.

Return values are PHP arrays.

All structures are implemented directly as PHP arrays.
An array is considered to be 'associative' (e.g. like a JSON 'object')
if it contains at least one non-numeric key.

Because of this, empty arrays ([]) and empty objects ({}) compare
the same, and (for instance) an 'add' of a string key to an empty
array will succeed in this implementation where it might fail in
others.

$simplexml_mode is provided to help with working with arrays produced
from XML in the style of simplexml - e.g. repeated XML elements are
expressed as arrays.  When $simplexml_mode is enabled, leaves with
scalar values are implicitly treated as length-1 arrays, so this
test will succeed:

    { "comment": "basic simplexml array promotion",
      "doc": { "foo":1 },
      "patch": [ {"add":"/foo/1", "value":2} ],
      "expected": { "foo":[1, 2] } },

Also, when $simplexml_mode is true, 1-length arrays are converted to scalars
on return from patch().

*/


class JsonPatchException extends Exception { }


class JsonPatch
{
  // Follow a json-pointer address into a JSON document and return
  // the designated leaf value
  public static function get($doc, $pointer, $simplexml_mode=false)
  {
    $doc = self::json_to_array($doc);
    $pointer = self::json_to_array($pointer);

    $parts = self::decompose_pointer($pointer);
    return self::get_helper($doc, $parts, $simplexml_mode);
  }


  // Compute a list of json-patch structures representing the diff
  // between $src and $dst
  public static function diff($src, $dst)
  {
    $src = self::json_to_array($src);
    $dst = self::json_to_array($dst);

    return self::diff_values("", $src, $dst);    
  }


  // Compute a new document from the supplied $doc and $patches.
  public static function patch($doc, $patches, $simplexml_mode=false)
  {
    $doc = self::json_to_array($doc);
    $patches = self::json_to_array($patches);

    // accept singleton patches
    if (count($patches) != 0 && !isset($patches[0]))
    {
      $patches = Array($patches);
    }

    foreach ($patches as $patch)
    {
      $op = trim($patch['op']); //one test in tests.json defines "op":" add"

      if (empty($op)) {
        throw new JsonPatchException("Operation member 'op' missing in "
          . json_encode($patch));
      }

      if (!in_array($op, array('add', 'remove', 'replace', 'move', 'copy', 'test'))) {
        throw new JsonPatchException("Unrecognized operation $op in "
          . json_encode($patch));
      }
      
      $parts = self::decompose_pointer($patch['path']);
      if (in_array($op, Array('test', 'add', 'replace')))
      {
        $value = $patch['value'];
      }
      if (in_array($op, Array('move', 'copy')))
      {
        $to_parts = self::decompose_pointer($patch['to']);
      }
      
      if ($op === 'add')
      {
        $doc = self::do_op($doc, $op, $parts, $value, $simplexml_mode);
      }
      else if ($op == 'replace')
      {
        $doc = self::do_op($doc, $op, $parts, $value, $simplexml_mode);
      }
      else if ($op == 'remove')
      {
        $doc = self::do_op($doc, $op, $parts, null, $simplexml_mode);
      }
      
      else if ($op == 'test')
      {
        self::test($doc, $parts, $value, $simplexml_mode);
      }
      
      else if ($op == 'copy')
      {
        $value = self::get_helper($doc, $parts, $simplexml_mode);
        $doc = self::do_op($doc, 'add', $to_parts, $value, $simplexml_mode);
      }
      else if ($op == 'move')
      {
        $value = self::get_helper($doc, $parts, $simplexml_mode);
        $doc = self::do_op($doc, 'remove', $parts, null, $simplexml_mode);
        $doc = self::do_op($doc, 'add', $to_parts, $value, $simplexml_mode);
      }
    }

    if ($simplexml_mode)
      $doc = self::re_singletize($doc);
    
    return $doc;
  }
  

  public static function compose_pointer($parts)
  {
    $result = "";
    foreach($parts as $part)
    {
      $part = str_replace('~', '~0', $part);
      $part = str_replace('/', '~1', $part);
      $result = $result . "/" . $part;
    }
    return $result;
  }

  
  public static function escape_pointer_part($part)
  {
    $part = str_replace('~', '~0', $part);
    $part = str_replace('/', '~1', $part);
    return $part;
  }


  // Private functions follow
  
  // This one just does leaves
/*   private static function ds2($doc) { */
/*     if (!is_array($doc)) */
/*       return($doc); */
/*     $result = array(); */
/*     foreach(array_keys($doc) as $key) */
/*     { */
/*       if (self::is_associative($doc) */
/*           && !(is_array($doc[$key]) || self::is_associative($doc[$key]))) */
/*       { */
/*         $result[$key] = array(self::ds2($doc[$key])); */
/*       } */
/*       else */
/*       { */
/*         $result[$key] = self::ds2($doc[$key]); */
/*       } */
/*     } */
/*     return $result; */
/*   } */

  // Walk through an associative array and make every value
  // into an array if it isn't one already.
/*   private static function de_singletize($doc, $in_array=false) */
/*   { */
/*     $result = array(); */
/*     foreach(array_keys($doc) as $key) */
/*     { */
/*       if (!is_array($doc[$key])) */
/*       { */
/*         if ($in_array) */
/*         { */
/*           $result[$key] = $doc[$key]; */
/*         } */
/*         else */
/*         { */
/*           $result[$key] = array($doc[$key]); */
/*         } */
/*       } */
/*       else if (self::is_associative($doc[$key])) */
/*       { */
/*         if ($in_array) { */
/*           $result[$key] = self::de_singletize($doc[$key]); */
/*         } else { */
/*           $result[$key] = array(self::de_singletize($doc[$key])); */
/*         } */
/*       } */
/*       else */
/*       { */
/*         $result[$key] = self::de_singletize($doc[$key], true); */
/*       } */
/*     } */
/*     return $result; */
/*   } */

/*  <frotz/> -> 0-length array, whereas*/
/*  <fizz>wizz</fizz> -> "wizz" */


  // Walk through the doc and turn every 1-length array into a
  // singleton value.  This follows SimpleXML behavior.
  private static function re_singletize($doc)
  {
    if (!is_array($doc))
      return $doc;

    if (isset($doc[0]) && count($doc) == 1)
      return self::re_singletize($doc[0]);

    $result = array();
    foreach(array_keys($doc) as $key)
    {
      $result[$key] = self::re_singletize($doc[$key]);
    }
    return $result;
  }


  private static function json_to_array($json)
  {
    if (!is_array($json))
    {
      $json_str = $json;
      $json = json_decode($json_str, 1);
      if (is_null($json))
      {
        throw new JsonPatchException("Invalid JSON: " . $json_str);
      }
    }
    return $json;
  }


  private static function decompose_pointer($pointer)
  {
    $parts = explode('/', $pointer);
    if (array_shift($parts) !== "")
    {
      throw new JsonPatchException("Location must start with / in $pointer");
    }
    for ($i = 0; $i < count($parts); $i++)
    {
      $parts[$i] = str_replace('~1', '/', $parts[$i]);
      $parts[$i] = str_replace('~0', '~', $parts[$i]);
    }
    return $parts;
  }

  
  // diff support functions
  
  
  // Dispatch to a recursive diff_assoc or diff_array call if needed,
  // or emit a patch to replace the current value.
  private static function diff_values($path, $value, $other)
  {
    // manually handle the {}-looks-like-[] case, when other is associative
    if ((count($value) == 0 || count($other) == 0)
        && (self::is_associative($value) || self::is_associative($other)))
    {
      return self::diff_assoc($path, $value, $other);
    }
    else if (self::is_associative($value) && self::is_associative($other))
    {
      return self::diff_assoc($path, $value, $other);
    }
    else if (is_array($value) && !self::is_associative($value)
             && is_array($other) && !self::is_associative($value))
    {
      return self::diff_array($path, $value, $other);
    }
    else
    {
      if ($value !== $other)
      {
        return array(array("replace" => "$path", "value" => $other));
      }
    }
    return array();
  }
  
  
  // Walk associative arrays $src and $dst, returning a list of patches
  private static function diff_assoc($path, $src, $dst)
  {
    $result = array();
    foreach (array_keys($src) as $key)
    {
      $ekey = self::escape_pointer_part($key);
      if (!array_key_exists($key, $dst))
      {
        $result[] = array("remove" => "$path/$ekey");
      }
      else
      {
        $result = array_merge($result,
                              self::diff_values("$path/$ekey",
                                                $src[$key], $dst[$key]));
      }
    }
    foreach (array_keys($dst) as $key)
    {
      if (!array_key_exists($key, $src))
      {
        $ekey = self::escape_pointer_part($key);
        $result[] = array("add" => "$path/$ekey", "value" => $dst[$key]);
      }
    }
    return $result;
  }


  // Walk simple arrays $src and $dst, returning a list of patches
  private static function diff_array($path, $src, $dst)
  {
    $result = array();
    $lsrc = count($src);
    $ldst = count($dst);
    $max = ($lsrc > $ldst) ? $lsrc : $ldst;

    // Walk backwards through arrays, starting with longest
    $i = $max - 1;
    while ($i >= 0) // equivalent for loop didn't work?
    {
      if ($i < $lsrc && $i < $ldst)
      {
        $result = array_merge($result,
                              self::diff_values("$path/$i",
                                                $src[$i], $dst[$i]));
      }
      else if ($i < $ldst)
      {
        $result[] = array("add" => "$path/$i", "value" => $dst[$i]);
      }
      else if ($i < $lsrc)
      {
        $result[] = array("remove" => "$path/$i");
      }
      $i--;
    }
    return $result;
  }


  // patch support functions


  // Implements the 'test' op
  private static function test($doc, $parts, $value, $simplexml_mode)
  {
    $found = self::get_helper($doc, $parts, $simplexml_mode);

    if (!self::arrays_are_equal($found, $value))
    {
      throw new JsonPatchException("'test' target value different - expected "
                                   . json_encode($value) . ", found "
                                   . json_encode($found));
    }
  }

  
  // Helper for get() and 'copy', 'move', 'test' ops - get a value from a doc.
  private static function get_helper($doc, $parts, $simplexml_mode)
  {
    $part = array_shift($parts);
    if (!array_key_exists($part, $doc))
    {
      throw new JsonPatchException("Location '$part' not found in target doc");
    }
    if (count($parts) > 0)
    {
      if ($simplexml_mode
          && count($parts) == 1 // XXXmm needed?
          && ($parts[0] == '0' || $parts[0] == '1')
          && self::is_associative($doc) && !is_array($doc[$part]))
      {
        return self::get_helper(array($doc[$part]), $parts, $simplexml_mode);
      }
      else
      {
        return self::get_helper($doc[$part], $parts, $simplexml_mode);
      }
    }
    else
    {
      return $doc[$part];
    }
  }


  // Test whether a php array looks 'associative' - does it have
  // any non-numeric keys?
  // 
  // note: is_associative(array()) === false
  private static function is_associative($a)
  {
    if (!is_array($a))
    {
      return false;
    }
    foreach (array_keys($a) as $key)
    {
      if (is_string($key))
      {
        return true;
      }
    }
    return false;
  }


  private static function recursive_ksort($array) {
    if (!is_array($array))
    {
      return $array;
    }
    ksort($array);
    foreach (array_keys($array) as $key) {
      $array[$key] = self::recursive_ksort($array[$key]);
    }
    return $array;
  }


  private static function arrays_are_equal($a1, $a2)
  {
    if (is_array($a1)) $a1 = self::recursive_ksort($a1);
    if (is_array($a2)) $a2 = self::recursive_ksort($a2);
    return json_encode($a1) === json_encode($a2);
  }


  // Apply a single op to modify the given document.
  //
  // As php arrays are not passed by reference, this function works
  // recursively, rebuilding complete subarrays that need changing;
  // the revised subarray is changed in the parent array before returning it.
  private static function do_op($doc, $op, $parts, $value, $simplexml_mode)
  {
    // recur until we get to the target
    $part = array_shift($parts);

    if (count($parts) > 0)
    {
      if (!array_key_exists($part, $doc))
      {
        throw new JsonPatchException("'$op' location '$part' not in target doc");
      }
      // recur, adding resulting sub-doc into doc returned to caller

      // special case for simplexml-style behavior - make singleton
      // scalar leaves look like 1-length arrays
      if ($simplexml_mode
          && count($parts) == 1 // XXXmm needed?
          && ($parts[0] == '0' || $parts[0] == '1')
          && self::is_associative($doc) && !is_array($doc[$part]))
      {
        $doc[$part] = self::do_op(array($doc[$part]), $op, $parts, $value,
                                  $simplexml_mode);
      }
      else
      {
        $doc[$part] = self::do_op($doc[$part], $op, $parts, $value,
                                  $simplexml_mode);
      }
      return $doc;
    }

    // at target
    if (!is_array($doc))
    {
      throw new JsonPatchException('Target must be array or associative array');
    }

    if (!self::is_associative($doc)) // N.B. returns false for empty arrays
    {
      if (count($doc) && !is_numeric($part))
      {
      throw new JsonPatchException('Non-array operation on array target');
      }
      else
      {
        // check range, if numeric
        if (is_numeric($part) &&
            ($part < 0 || (($op == 'remove' && $part >= count($doc))
                           || ($op != 'remove' && $part > count($doc)))))
        {
          throw new JsonPatchException("Can't operate outside of array bounds");
        }
      }
    }

    if ($op == 'add')
    {
      if (!self::is_associative($doc) && is_numeric($part))
      {
        array_splice($doc, $part, 0, Array($value));
      }
      else
      {
        if (array_key_exists($part, $doc) && !is_null($doc[$part]))
        {
          throw new JsonPatchException("'add' target '$part' already set");
        }
        $doc[$part] = $value;
      }
    }

    else if ($op == 'replace')
    {
      if (is_numeric($part))
      {
        array_splice($doc, $part, 1, Array($value));
      }
      else
      {
        if (!array_key_exists($part, $doc))
        {
          throw new JsonPatchException("'replace' target '$part' not already set");
        }
        $doc[$part] = $value;
      }
    }

    else if ($op == 'remove')
    {
      if (is_numeric($part))
      {
        array_splice($doc, $part, 1);
      }
      else
      {
        if (!array_key_exists($part, $doc))
        {
          throw new JsonPatchException("'remove' target '$part' not already set");
        }
        unset($doc[$part]);
      }
    }
    return $doc;
  }


  // Compute a new document from the supplied $doc and $patches.
  public static function patch_xml($doc, $patches)
  {
    $doc = self::json_to_array($doc);
    $patches = self::json_to_array($patches);

    // accept singleton patches
    if (count($patches) != 0 && !isset($patches[0]))
    {
      $patches = Array($patches);
    }

    foreach ($patches as $patch)
    {
      $op = trim($patch['op']); //one test in tests.json defines "op":" add"

      if (empty($op)) {
        throw new JsonPatchException("Operation member 'op' missing in "
          . json_encode($patch));
      }

      if (!in_array($op, array('add', 'remove', 'replace', 'move', 'copy', 'test'))) {
        throw new JsonPatchException("Unrecognized operation $op in "
          . json_encode($patch));
      }

      $parts = self::decompose_pointer($patch['path']);
      if (in_array($op, Array('test', 'add', 'replace')))
      {
        $value = $patch['value'];
      }
      if (in_array($op, Array('move', 'copy')))
      {
        $to_parts = self::decompose_pointer($patch['to']);
      }
      
      if ($op === 'add')
      {
        $doc = self::do_op_xml($doc, $op, $parts, $value);
      }
    }

    return $doc;
  }



  // Apply a single op to modify the given document.
  private static function do_op_xml($doc, $op, $parts, $value=null)
  {
    // recur until we get to the target
    $part = array_shift($parts);
    if (count($parts) > 0)
    {
      if (!isset($doc->{$part}))
      {
        throw new JsonPatchException("'$op' location '$part' not in target doc");
      }
      // recur, adding resulting sub-doc into doc returned to caller
      self::do_op_xml($doc->{$part}, $op, $parts, $value);
      return $doc;
    }

    if ($op == 'add')
    {
      // XXX numeric case needed
      {
        if (isset($doc->{$part}))
        {
          // replace this, as nu semantics are:
          // if it exists, and the thingping is numeric, then expando.

          throw new JsonPatchException("'add' target '$part' already set");
        }
        $doc->addChild($part, $value);
      }
    }
    return $doc;
  }
}
