#!/bin/bash

   echo "starting test_arch_restore"
   rm -rf test_restore_dir/test_source1
   ./arch_restore test_archive test_restore_dir "test_source1.*"
   diff -qr test_restore_dir/test_source1/ test_source1 >& test_restore_dir/diffs
   diff test_restore_dir/diffs test_restore_dir/diffs_expected
   echo "completed test_arch_restore, if no diffs listed above then it passed"


