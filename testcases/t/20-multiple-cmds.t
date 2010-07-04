#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests multiple commands (using ';') and multiple operations (using ',')
#
use i3test tests => 24;
use X11::XCB qw(:all);

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

sub multiple_cmds {
    my ($cmd) = @_;

    $i3->command('open')->recv;
    $i3->command('open')->recv;
    ok(@{get_ws_content($tmp)} == 2, 'two containers opened');

    $i3->command($cmd)->recv;
    ok(@{get_ws_content($tmp)} == 0, "both containers killed (cmd = $cmd)");
}
multiple_cmds('kill;kill');
multiple_cmds('kill; kill');
multiple_cmds('kill ; kill');
multiple_cmds('kill ;kill');
multiple_cmds('kill  ;kill');
multiple_cmds('kill  ;  kill');
multiple_cmds("kill;\tkill");
multiple_cmds("kill\t;kill");
multiple_cmds("kill\t;\tkill");
multiple_cmds("kill\t ;\tkill");
multiple_cmds("kill\t ;\t kill");
multiple_cmds("kill \t ; \t kill");

# TODO: need a non-invasive command before implementing a test which uses ','

diag( "Testing i3, Perl $], $^X" );
