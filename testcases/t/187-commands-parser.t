#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests the standalone parser binary to see if it calls the right code when
# confronted with various commands, if it prints proper error messages for
# wrong commands and if it terminates in every case.
#
use i3test i3_autostart => 0;

sub parser_calls {
    my ($command) = @_;

    # TODO: use a timeout, so that we can error out if it doesn’t terminate
    # TODO: better way of passing arguments
    my $stdout = qx(../test.commands_parser '$command');

    # Filter out all debugging output.
    my @lines = split("\n", $stdout);
    @lines = grep { not /^# / } @lines;

    # The criteria management calls are irrelevant and not what we want to test
    # in the first place.
    @lines = grep { !(/cmd_criteria_init()/ || /cmd_criteria_match_windows/) } @lines;
    return join("\n", @lines);
}

################################################################################
# 1: First that the parser properly recognizes commands which are ok.
################################################################################

is(parser_calls('move workspace 3'),
   'cmd_move_con_to_workspace_name(3)',
   'single number (move workspace 3) ok');

is(parser_calls('move to workspace 3'),
   'cmd_move_con_to_workspace_name(3)',
   'to (move to workspace 3) ok');

is(parser_calls('move window to workspace 3'),
   'cmd_move_con_to_workspace_name(3)',
   'window to (move window to workspace 3) ok');

is(parser_calls('move container to workspace 3'),
   'cmd_move_con_to_workspace_name(3)',
   'container to (move container to workspace 3) ok');

is(parser_calls('move workspace foobar'),
   'cmd_move_con_to_workspace_name(foobar)',
   'single word (move workspace foobar) ok');

is(parser_calls('move workspace 3: foobar'),
   'cmd_move_con_to_workspace_name(3: foobar)',
   'multiple words (move workspace 3: foobar) ok');

is(parser_calls('move workspace "3: foobar"'),
   'cmd_move_con_to_workspace_name(3: foobar)',
   'double quotes (move workspace "3: foobar") ok');

is(parser_calls('move workspace "3: foobar, baz"'),
   'cmd_move_con_to_workspace_name(3: foobar, baz)',
   'quotes with comma (move workspace "3: foobar, baz") ok');

is(parser_calls('move workspace 3: foobar, nop foo'),
   "cmd_move_con_to_workspace_name(3: foobar)\n" .
   "cmd_nop(foo)",
   'multiple ops (move workspace 3: foobar, nop foo) ok');

is(parser_calls('exec i3-sensible-terminal'),
   'cmd_exec((null), i3-sensible-terminal)',
   'exec ok');

is(parser_calls('exec --no-startup-id i3-sensible-terminal'),
   'cmd_exec(--no-startup-id, i3-sensible-terminal)',
   'exec --no-startup-id ok');

is(parser_calls('resize shrink left'),
   'cmd_resize(shrink, left, 10, 10)',
   'simple resize ok');

is(parser_calls('resize shrink left 25 px'),
   'cmd_resize(shrink, left, 25, 10)',
   'px resize ok');

is(parser_calls('resize shrink left 25 px or 33 ppt'),
   'cmd_resize(shrink, left, 25, 33)',
   'px + ppt resize ok');

is(parser_calls('resize shrink left 25 px or 33 ppt'),
   'cmd_resize(shrink, left, 25, 33)',
   'px + ppt resize ok');

is(parser_calls('resize shrink left 25 px or 33 ppt,'),
   'cmd_resize(shrink, left, 25, 33)',
   'trailing comma resize ok');

is(parser_calls('resize shrink left 25 px or 33 ppt;'),
   'cmd_resize(shrink, left, 25, 33)',
   'trailing semicolon resize ok');

is(parser_calls('resize shrink left 25'),
   'cmd_resize(shrink, left, 25, 10)',
   'resize early end ok');

is(parser_calls('[con_mark=yay] focus'),
   "cmd_criteria_add(con_mark, yay)\n" .
   "cmd_focus()",
   'criteria focus ok');

is(parser_calls("[con_mark=yay con_mark=bar] focus"),
   "cmd_criteria_add(con_mark, yay)\n" .
   "cmd_criteria_add(con_mark, bar)\n" .
   "cmd_focus()",
   'criteria focus ok');

is(parser_calls("[con_mark=yay\tcon_mark=bar] focus"),
   "cmd_criteria_add(con_mark, yay)\n" .
   "cmd_criteria_add(con_mark, bar)\n" .
   "cmd_focus()",
   'criteria focus ok');

is(parser_calls("[con_mark=yay\tcon_mark=bar]\tfocus"),
   "cmd_criteria_add(con_mark, yay)\n" .
   "cmd_criteria_add(con_mark, bar)\n" .
   "cmd_focus()",
   'criteria focus ok');

is(parser_calls('[con_mark="yay"] focus'),
   "cmd_criteria_add(con_mark, yay)\n" .
   "cmd_focus()",
   'quoted criteria focus ok');

################################################################################
# 2: Verify that the parser spits out the right error message on commands which
# are not ok.
################################################################################

is(parser_calls('unknown_literal'),
   "Expected one of these tokens: <end>, '[', 'move', 'exec', 'exit', 'restart', 'reload', 'border', 'layout', 'append_layout', 'workspace', 'focus', 'kill', 'open', 'fullscreen', 'split', 'floating', 'mark', 'resize', 'nop', 'scratchpad', 'mode'\n" .
   "Your command: unknown_literal\n" .
   "              ^^^^^^^^^^^^^^^",
   'error for unknown literal ok');

is(parser_calls('move something to somewhere'),
   "Expected one of these tokens: 'window', 'container', 'to', 'workspace', 'output', 'scratchpad', 'left', 'right', 'up', 'down'\n" .
   "Your command: move something to somewhere\n" .
   "                   ^^^^^^^^^^^^^^^^^^^^^^",
   'error for unknown literal ok');

done_testing;
