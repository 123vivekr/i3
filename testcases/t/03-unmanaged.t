#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 5;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;

BEGIN {
    use_ok('X11::XCB::Window');
}

X11::XCB::Connection->connect(':0');

my $original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);

my $window = X11::XCB::Window->new(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => $original_rect,
    override_redirect => 1,
    background_color => 12632256
);

isa_ok($window, 'X11::XCB::Window');

is_deeply($window->rect, $original_rect, "rect unmodified before mapping");

$window->create;
$window->map;

my $new_rect = $window->rect;
isa_ok($new_rect, 'X11::XCB::Rect');

is_deeply($new_rect, $original_rect, "window untouched");

diag( "Testing i3, Perl $], $^X" );
