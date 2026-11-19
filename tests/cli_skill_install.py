from pathlib import Path
import tempfile,subprocess,json,os,concurrent.futures
project=Path(__file__).resolve().parents[1]
binary=str(project/'build/quickref-viewer')
with tempfile.TemporaryDirectory() as temporary:
 root=Path(temporary).resolve();env=os.environ.copy();env['CODEX_HOME']=str(root/'codex')
 def install():return subprocess.run([binary,'skill','install','--json'],env=env,capture_output=True,text=True)
 with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:results=list(pool.map(lambda _:install(),range(8)))
 assert sum(r.returncode==0 for r in results)==1,[(r.returncode,r.stderr) for r in results]
 target=root/'codex/skills/quickref-viewer/SKILL.md';expected=(project/'src/agent-skill.md').read_bytes()
 assert target.read_bytes()==expected
 assert target.stat().st_mode & 0o777==0o600
 for r in results:assert json.loads(r.stdout if r.returncode==0 else r.stderr)['ok']==(r.returncode==0)
 target.write_text('User instructions must survive')
 result=install();assert result.returncode==1 and not json.loads(result.stderr)['ok'];assert target.read_text()=='User instructions must survive'
 target.unlink();outside=root/'outside';outside.write_text('Outside sentinel');target.symlink_to(outside)
 result=install();assert result.returncode==1 and outside.read_text()=='Outside sentinel' and target.is_symlink()
 target.unlink();missing=root/'missing';target.symlink_to(missing)
 assert install().returncode==1 and not missing.exists()
 print('CLI checks passed: eight concurrent installers, one winner; full bundled contents, mode 0600, JSON/exit status, existing-file and symlink preservation.')
