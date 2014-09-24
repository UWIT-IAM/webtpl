# parse this file with '#' comments enabled
#
# define aa and cc, but not bb
{AA}{BB}{CC}
----
{BB}{CC}
#{AA}
{AA}{BB}
.{CC}.
#end of sub
