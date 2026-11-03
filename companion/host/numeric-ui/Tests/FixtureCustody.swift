import Foundation

/// Test-only cooperative admission. Real Foundation Process, pipes and signals remain in use.
/// With no fixture supervisor selected this leaves the Python program unchanged.
enum FixtureCustody {
    static func wrap(_ source: String) -> String {
        let env = ProcessInfo.processInfo.environment
        guard let directory = env["SC_FIXTURE_CUSTODY_DIR"], env["SC_FIXTURE_CUSTODY_NONCE"] != nil else { return source }
        let ticket = UUID().uuidString
        let planned = URL(fileURLWithPath: directory).appendingPathComponent(ticket + ".planned")
        do { try Data(ticket.utf8).write(to: planned, options: .withoutOverwriting) }
        catch { preconditionFailure("fixture custody ticket failed") }
        // UUID contains only ASCII hex/hyphens. Directory/nonce travel via environment.
        return """
import os,signal,json
_sc_ticket='\(ticket)'
_sc_directory=os.environ['SC_FIXTURE_CUSTODY_DIR']
_sc_request={'ticket':_sc_ticket,'nonce':os.environ['SC_FIXTURE_CUSTODY_NONCE'],'pid':os.getpid(),'ppid':os.getppid(),'sid':os.getsid(0)}
_sc_fd=os.open(os.path.join(_sc_directory,_sc_ticket+'.pending'),os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
with os.fdopen(_sc_fd,'w') as _sc_file:
 _sc_file.write(json.dumps(_sc_request))
os.rename(os.path.join(_sc_directory,_sc_ticket+'.pending'),os.path.join(_sc_directory,_sc_ticket+'.request'))
os.kill(os.getpid(),signal.SIGSTOP)
if not os.path.isfile(os.path.join(_sc_directory,_sc_ticket+'.admitted')):raise RuntimeError('fixture resumed without custody')
""" + "\n" + source
    }
}
