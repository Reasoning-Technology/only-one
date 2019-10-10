#!/bin/bash

set -x
psql $1 -f schema_teardown.psql -f schema.psql
