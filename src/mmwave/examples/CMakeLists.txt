set(base_examples
    mmwave-example
    mmwave-tcp-example
    mmwave-simple-epc
    mmwave-simple-epc-ipv6
    mmwave-epc-tdma
    mmwave-tdma
    mmwave-simple-building-obstacle
    mmwave-tcp-building-example
    mmwave-amc-test
    mmwave-amc-test2
    mmwave-epc-amc-test
    mc-twoenbs
    mc-twoenbs-ipv6
    mmwave-ca-same-bandwidth
    mmwave-ca-diff-bandwidth
    mmwave-beamforming-codebook-example
    iab-install-test
    iab-mt-to-donor-attachment-test
    iab-node-depth-test
    iab-to-iab-attachment-test
    mt-and-du-data-test
    ue-du-data-test
    mt-and-du-two-iabs-data-test
    iab-scheduling-test
    bap-receiving-test
    iab-e2e-data-1-node-test
    iab-3-nodes-test
    iab-inmarsat-scenario-1
    iab-scheduling-flexible-slot-test
    iab-dl-flow-test
)

foreach(
  example
  ${base_examples}
)
  build_lib_example(
    NAME ${example}
    SOURCE_FILES ${example}.cc
    LIBRARIES_TO_LINK
      ${libmmwave}
      ${libantenna}
      ${libspectrum}
  )
endforeach()

if(${ENABLE_QD_CHANNEL})
  build_lib_example(
    NAME qd-channel-full-stack-example
    SOURCE_FILES qd-channel-full-stack-example.cc
    LIBRARIES_TO_LINK
      ${libmmwave}
      ${libqd-channel}
  )
endif()
