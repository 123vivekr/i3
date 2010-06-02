#!perl
# vim:ts=4:sw=4:expandtab

use i3test tests => 3;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;
my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

#####################################################################
# Create a parent window
#####################################################################

my $window = $x->root->create_child(
class => WINDOW_CLASS_INPUT_OUTPUT,
rect => [ 0, 0, 30, 30 ],
background_color => '#C0C0C0',
);

$window->name('Parent window');
$window->map;

sleep 0.25;

#########################################################################
# Switch workspace to 10 and open a child window. It should be positioned
# on workspace 9.
#########################################################################
my $otmp = get_unused_workspace();
$i3->command("workspace $otmp")->recv;

my $child = $x->root->create_child(
class => WINDOW_CLASS_INPUT_OUTPUT,
rect => [ 0, 0, 30, 30 ],
background_color => '#C0C0C0',
);

$child->name('Child window');
$child->client_leader($window);
$child->map;

sleep 0.25;

isnt($x->input_focus, $child->id, "Child window focused");

# Switch back
$i3->command("workspace $tmp")->recv;

is($x->input_focus, $child->id, "Child window focused");
