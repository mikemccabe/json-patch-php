json-patch-php
================

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
json_decode($json_string, 1)

All structures are implemented directly as PHP arrays.
An array is considered to be 'associative' (e.g. like a JSON 'object')
if it contains at least one non-numeric key.

Because of this, empty arrays ([]) and empty objects ({}) compare
the same, and (for instance) an 'add' of a string key to an empty
array will succeed in this implementation where it might fail in
others.
