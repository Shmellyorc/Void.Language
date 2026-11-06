import qualify as h
import json,shutil,time
# Fixed additional boundary coverage; avoid duplicating the 63-test common set.
selected=['log_frontend','log_files','log_oracles','log_cache','log_console','debug_cache_processes','automatic_cache_processes','io_execution','io_async_execution','io_wire_normal','io_wire_optimized','io_native_cancelled','io_native_shutdown','io_native_close_failure','io_native_opening_shutdown','io_native_whole_shutdown','io_async_acquisition','http_reload','http_source_faults','http_timeouts','async_ir','io_ir']
targets=['voidc','void_io_wire_normal','void_io_wire_optimized','void_io_native','void_io_runner','void_http_reload','void_http_source_runner','void_http_timeouts','void_async_tests','void_io_ir']
for profile in ['release','debug','strict']:
 while not (h.E/(profile+'-LastTest.log')).exists():time.sleep(5)
 build=h.ROOT/('build/llvm23-'+profile)
 if h.run(profile+'-boundary-build',['cmake','--build',build,'-j2','--target',*targets],small=False).returncode:continue
 (h.E/(profile+'-boundary-selected.json')).write_text(json.dumps(selected,indent=2))
 h.run(profile+'-boundary-ctest',['ctest','--test-dir',build,'--output-on-failure','--output-junit',h.E/(profile+'-boundary-junit.xml'),'-j2','-R','^('+'|'.join(selected)+')$'])
 shutil.copy2(build/'Testing/Temporary/LastTest.log',h.E/(profile+'-boundary-LastTest.log'))

