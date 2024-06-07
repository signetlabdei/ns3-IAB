#! /usr/bin/env python
# -*- coding: utf-8 -*-
## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# A list of C++ examples to run in order to ensure that they remain
# buildable and runnable over time.  Each tuple in the list contains
#
#     (example_name, do_run, do_valgrind_run).
#
# See test.py for more information.
cpp_examples = [
    ("mc-twoenbs-ipv6", "True", "True"),
    ("mc-twoenbs", "True", "True"),
    ("mmwave-amc-test", "True", "True"),
    ("mmwave-amc-test2", "True", "True"),
    ("mmwave-ca-diff-bandwidth", "True", "True"),
    ("mmwave-ca-same-bandwidth", "True", "True"),
    ("mmwave-epc-amc-test", "True", "True"),
    ("mmwave-epc-tdma", "True", "True"),
    ("mmwave-example", "True", "True"),
    ("mmwave-simple-building-obstacle", "True", "True"),
    ("mmwave-simple-epc-ipv6", "True", "True"),
    ("mmwave-simple-epc", "True", "True"),
    ("mmwave-tcp-building-example", "True", "True"),
    ("mmwave-tcp-example", "True", "True"),
    ("iab-install-test", "True", "True"),
    ("iab-mt-to-donor-attachment-test", "True", "True"),
    ("iab-node-depth-test", "True", "True"),
    ("mt-and-du-data-test", "True", "True"),
    ("iab-to-iab-attachment-test", "True", "True"),
    #("ue-du-data-test", "True", "True"),
    ("mt-and-du-two-iabs-data-test", "True", "True"),
    ("mmwave-tdma", "True", "True"),
    ("iab-scheduling-test", "True", "True"),
    ("bap-receiving-test", "True", "True"),
    ("iab-3-nodes-test", "True", "True"),
    ("iab-inmarsat-scenario-1", "True", "True"),
    ("iab-scheduling-flexible-slot-test", "True", "True"),
    ("iab-dl-flow-depth-1-test", "True", "True"),
    ("iab-dl-flow-depth-2-test", "True", "True"),
    ("iab-paper-scenario-fdm", "True", "True")
]

# A list of Python examples to run in order to ensure that they remain
# runnable over time.  Each tuple in the list contains
#
#     (example_name, do_run).
#
# See test.py for more information.
python_examples = []
