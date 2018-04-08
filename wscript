# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

def configure(conf):
    have_bm = conf.check_cfg(package='bm', uselib_store='BM',
                                  args=['--cflags', '--libs'],
                                  mandatory=True)
    have_boost = conf.check_cfg(package='boost_system',uselib_store='BOOST' ,
                                  args=['--cflags', '--libs'],
                                  mandatory=True)
    have_sw = conf.check_cfg(package='simple_switch',uselib_store='SW' ,
                                  args=['--cflags', '--libs'],
                                  mandatory=True)

#    conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')
#    conf.env.append_value("LINKFLAGS", ["-L/usr/local/lib","bmall"])
def build(bld):
    all_modules = [mod[len("ns3-"):] for mod in bld.env['NS3_ENABLED_MODULES']]
    module = bld.create_ns3_module('keysight', ['antenna', 'aodv', 'applications', 'bridge', 'buildings', 'config-store', 'core', 'csma', 'csma-layout', 'dsdv', 'dsr', 'energy', 'fd-net-device', 'flow-monitor', 'internet', 'internet-apps', 'lr-wpan', 'lte', 'mesh', 'mobility', 'mpi', 'netanim', 'network', 'nix-vector-routing', 'olsr', 'point-to-point', 'point-to-point-layout', 'propagation', 'sixlowpan', 'spectrum', 'stats', 'test', 'topology-read', 'traffic-control', 'uan', 'virtual-net-device', 'wave', 'wifi', 'wimax'])
    module.source = [
	'model/key-hash.cc'
        ]

    module_test = bld.create_ns3_module_test_library('keysight')
    module_test.source = [
        ]

    headers = bld(features='ns3header')
    headers.module = 'keysight'
    headers.source = [
	'model/key-hash.h'
        ]

    if bld.env['ENABLE_EXAMPLES']:
        bld.recurse('examples')
    # bld.ns3_python_bindings()
    module.use.append("BM")
    module.use.append("BOOST")
    module.use.append("SW")
