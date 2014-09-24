
Dynamic block used:
<!-- BEGIN DYNAMIC BLOCK: zzzz -->
Block1 {ABCD} - {EFGH} should appear
<!-- END DYNAMIC BLOCK: zzzz -->

Dynamic block unused:
<!-- BEGIN DYNAMIC BLOCK: zzz0 -->
Block1 {ABCD} - {EFGH} end (should not appear)
<!-- END DYNAMIC BLOCK: zzz0 -->

Nested dynamic blocks:
<!-- BDB: abc_d -->

  Block1 {ABCD} - {EFGH} end (should appear twice)

  <!-- BEGIN DYNAMIC BLOCK: efg -->
  Block1 {_EFG} - {_EFG} end (should appear only once)
  <!-- END DYNAMIC BLOCK: efg -->

<!-- unterminated block -->
  <!-- BEGIN DYNAMIC BLOCK: hijk -->
  Block1 {HIJK} - {HIJK} end (should not appear)

<!-- EDB: abc_d -->

#end of bad-tpl.tpl
