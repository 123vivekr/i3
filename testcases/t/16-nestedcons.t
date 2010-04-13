#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 8;
use Test::Deep;
use List::MoreUtils qw(all none);
use Data::Dumper;
use AnyEvent::I3;

my $i3 = i3("/tmp/nestedcons");

####################
# Request tree
####################

my $tree = $i3->get_workspaces->recv;
# $VAR1 = {
#           'fullscreen_mode' => 0,
#           'nodes' => [
#                         {
#                           'fullscreen_mode' => 0,
#                           'nodes' => [
#                                         {
#                                           'fullscreen_mode' => 0,
#                                           'nodes' => [],
#                                           'window' => undef,
#                                           'name' => '1',
#                                           'orientation' => 0,
#                                           'type' => 2
#                                         }
#                                       ],
#                           'window' => undef,
#                           'name' => 'LVDS1',
#                           'orientation' => 0,
#                           'type' => 1
#                         }
#                       ],
#           'window' => undef,
#           'name' => 'root',
#           'orientation' => 0,
#           'type' => 0
#         };

my $expected = {
    fullscreen_mode => 0,
    nodes => ignore(),
    window => undef,
    name => 'root',
    orientation => ignore(),
    type => 0,
    id => ignore(),
};

cmp_deeply($tree, $expected, 'root node OK');

my @nodes = @{$tree->{nodes}};

ok(@nodes > 0, 'root node has at least one leaf');

ok((all { $_->{type} == 1 } @nodes), 'all nodes are of type CT_OUTPUT');
ok((none { defined($_->{window}) } @nodes), 'no CT_OUTPUT contains a window');
ok((all { @{$_->{nodes}} > 0 } @nodes), 'all nodes have at least one leaf (workspace)');
my @workspaces;
for my $ws (map { @{$_->{nodes}} } @nodes) {
    push @workspaces, $ws;
}

ok((all { $_->{type} == 2 } @workspaces), 'all workspaces are of type CT_CON');
ok((all { @{$_->{nodes}} == 0 } @workspaces), 'all workspaces are empty yet');
ok((none { defined($_->{window}) } @workspaces), 'no CT_OUTPUT contains a window');

# TODO: get the focused container

$i3->command('open')->recv;

# TODO: get the focused container, check if it changed.
# TODO: get the old focused container, check if there is a new child

diag(Dumper(\@workspaces));

diag(Dumper($tree));


diag( "Testing i3, Perl $], $^X" );
