#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests the new_window and new_float config option.
#

use i3test;
use X11::XCB qw(:all);
use X11::XCB::Connection;

my $x = X11::XCB::Connection->new;

#####################################################################
# 1: check that new windows start with 'normal' border unless configured
# otherwise
#####################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $process = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_standard_window($x);

my @content = @{get_ws_content($tmp)};
ok(@content == 1, 'one container opened');
is($content[0]->{border}, 'normal', 'border normal by default');

exit_gracefully($process->pid);

#####################################################################
# 2: check that new tiling windows start with '1pixel' border when
# configured
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window 1pixel
EOT

$process = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_standard_window($x);

@content = @{get_ws_content($tmp)};
ok(@content == 1, 'one container opened');
is($content[0]->{border}, '1pixel', 'border normal by default');

exit_gracefully($process->pid);

#####################################################################
# 3: check that new floating windows start with 'normal' border unless
# configured otherwise
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

$process = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Create a floating window which is smaller than the minimum enforced size of i3
$first = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#C0C0C0',
    # replace the type with 'utility' as soon as the coercion works again in X11::XCB
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

$first->map;

sleep 0.25;

my $wscontent = get_ws($tmp);
my @floating = @{$wscontent->{floating_nodes}};
ok(@floating == 1, 'one floating container opened');
my $floatingcon = $floating[0];
is($floatingcon->{nodes}->[0]->{border}, 'normal', 'border normal by default');

exit_gracefully($process->pid);

#####################################################################
# 4: check that new floating windows start with '1pixel' border when
# configured
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_float 1pixel
EOT

$process = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Create a floating window which is smaller than the minimum enforced size of i3
$first = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#C0C0C0',
    # replace the type with 'utility' as soon as the coercion works again in X11::XCB
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

$first->map;

sleep 0.25;

$wscontent = get_ws($tmp);
@floating = @{$wscontent->{floating_nodes}};
ok(@floating == 1, 'one floating container opened');
$floatingcon = $floating[0];
is($floatingcon->{nodes}->[0]->{border}, '1pixel', 'border normal by default');

exit_gracefully($process->pid);

done_testing;
