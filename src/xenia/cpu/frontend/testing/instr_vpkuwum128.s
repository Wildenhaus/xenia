#vpkuwum128 isn't implemented yet
#test_vpkuwum128_1:
#  # {0, 1, 2, 3}
#  #_ REGISTER_IN v3 [00000000, 00000001, 00000002, 00000003]
#  # {4, 5, 6, 7}
#  #_ REGISTER_IN v4 [00000004, 00000005, 00000006, 00000007]
#  vpkuwum128 v5, v3, v4
#  blr
#  #_ REGISTER_OUT v3 [00000000, 00000001, 00000002, 00000003]
#  #_ REGISTER_OUT v4 [00000004, 00000005, 00000006, 00000007]
#  # {0, 1, 2, 3, 4, 5, 6, 7}
#  #_ REGISTER_OUT v5 [00000001, 00020003, 00040005, 00060007]

#test_vpkuwum128_2:
#  # {-4, -3, -2, -1}
#  #_ REGISTER_IN v3 [FFFFFFFC, FFFFFFFD, FFFFFFFE, FFFFFFFF]
#  # {0, 1, 2, 3}
#  #_ REGISTER_IN v4 [00000000, 00000001, 00000002, 00000003]
#  vpkuwum128 v5, v3, v4
#  blr
#  #_ REGISTER_OUT v3 [FFFFFFFC, FFFFFFFD, FFFFFFFE, FFFFFFFF]
#  #_ REGISTER_OUT v4 [00000000, 00000001, 00000002, 00000003]
#  # {-4, -3, -2, -1, 0, 1, 2, 3}
#  #_ REGISTER_OUT v5 [FFFCFFFD, FFFEFFFF, 00000001, 00020003]

#test_vpkuwum128_3:
#  # {0, 4294967295, 4294967295, 4294967295}
#  #_ REGISTER_IN v3 [00000000, FFFFFFFF, FFFFFFFF, FFFFFFFF]
#  # {4294967295, 0, 0, 0}
#  #_ REGISTER_IN v4 [FFFFFFFF, 00000000, 00000000, 00000000]
#  vpkuwum128 v5, v3, v4
#  blr
#  #_ REGISTER_OUT v3 [00000000, FFFFFFFF, FFFFFFFF, FFFFFFFF]
#  #_ REGISTER_OUT v4 [FFFFFFFF, 00000000, 00000000, 00000000]
#  # {0, 65535, 65535, 65535, 65535, 0, 0, 0}
#  #_ REGISTER_OUT v5 [0000FFFF, FFFFFFFF, FFFF0000, 00000000]
