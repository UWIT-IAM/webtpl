NOTE
  This part is commentary 
  and will not be transmitted to the html files
ENDNOTE
<!-- assignments
  {TPLMAC=tmplate assigned}
 -->
<html>
<head>
<title>Web Template test</title>
</head>
<body>

Simple replacement
..{REPL}..
<br>
++{REPL}++

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

  <!-- BEGIN DYNAMIC BLOCK: hijk -->
  Block1 {HIJK} - {HIJK} end (should not appear)
  <!-- END DYNAMIC BLOCK: hijk -->

<!-- EDB: abc_d -->
--{REPL}--
<p>

!- args ------------------------------------------------!

arg1 = {A1}
<!-- BDB: argv -->
arg2 = {A2}
<!-- EDB: argv -->
arg3 = {A3}

!-------------------------------------------------------!

#start of SUB
{SUB}
#end of SUB

#start of SUB3
{SUB3}
#end of SUB3

Value of AA: {AA}
Value of CC: {CC}
</html>
#end of test1.tpl
