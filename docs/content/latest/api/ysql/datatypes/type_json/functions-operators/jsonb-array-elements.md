---
title: jsonb_array_elements()
linkTitle: jsonb_array_elements()
summary: jsonb_array_elements() and json_array_elements()
description: jsonb_array_elements() and json_array_elements()
menu:
  latest:
    identifier: jsonb-array-elements
    parent: functions-operators
    weight: 60
isTocNested: true
showAsideToc: true
---

Here is the signature for the `jsonb` variant:

```
input value        jsonb
return value       SETOF jsonb
```

The functions in this pair require that the supplied JSON value is an _array_, and transform the list into a table. They are the counterparts, for an _array_ of primitive JSON values, to `jsonb_populate_recordset()` for JSON _objects_.

```postgresql
do $body$
declare
  j_array constant jsonb := '["cat", "dog house", 42, true, null, {"x": 17}]';
  j jsonb;

  elements jsonb[];
  expected_elements constant jsonb[] :=
    array[
      '"cat"'::jsonb,
      '"dog house"'::jsonb,
      '42'::jsonb,
      'true'::jsonb,
      'null'::jsonb,
      '{"x": 17}'::jsonb
    ];

  n int := 0;
begin
  for j in (select * from jsonb_array_elements(j_array)) loop
    n := n + 1;
    elements[n] := j;
  end loop;

  assert
    elements = expected_elements,
  'unexpected';
end;
$body$;
```