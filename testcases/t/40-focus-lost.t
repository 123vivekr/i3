#!perl
# vim:ts=4:sw=4:expandtab
# Regression: Check if the focus stays the same when switching the layout
# bug introduced by 77d0d42ed2d7ac8cafe267c92b35a81c1b9491eb
use i3test tests => 4;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);

BEGIN {
    use_ok('X11::XCB::Window');
}

my $i3 = i3("/tmp/nestedcons");
my $x = X11::XCB::Connection->new;

sub check_order {
    my ($msg) = @_;

    my @ws = @{$i3->get_workspaces->recv};
    my @nums = map { $_->{num} } grep { defined($_->{num}) } @ws;
    my @sorted = sort @nums;

    cmp_deeply(\@nums, \@sorted, $msg);
}

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

my $left = open_standard_window($x);
sleep 0.25;
my $mid = open_standard_window($x);
sleep 0.25;
my $right = open_standard_window($x);
sleep 0.25;

diag("left = " . $left->id . ", mid = " . $mid->id . ", right = " . $right->id);

is($x->input_focus, $right->id, 'Right window focused');

$i3->command('prev h')->recv;

is($x->input_focus, $mid->id, 'Mid window focused');

$i3->command('layout stacked')->recv;

is($x->input_focus, $mid->id, 'Mid window focused');
