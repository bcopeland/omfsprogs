#! /bin/bash
#
# various assert functions
function assert_ne
{
    if [[ $1 -eq $2 ]]; then
        echo "FAIL: \"$1 != $2\" $3"
    else
	echo "pass"
    fi
}

function assert_ne_s
{
    if [[ $1 -eq $2 ]]; then
        echo "FAIL: \"$1 != $2\" $3"
    fi
}

function assert_eq
{
    if [[ $1 -ne $2 ]]; then
        echo "FAIL: \"$1 == $2\" $3"
    else
	echo "pass"
    fi
}

function assert_eq_s
{
    if [[ $1 -ne $2 ]]; then
        echo "FAIL: \"$1 == $2\" $3"
    fi
}

function assertz
{
    assert_eq $1 0 $2
}

function assertz_s
{
    assert_eq_s $1 0 $2
}

function assert
{
    assert_ne $1 0 $2
}

function assert_s
{
    assert_ne_s $1 0 $2
}
