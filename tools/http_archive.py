"""Acquire an immutable HTTP dependency; retain rejected bytes and diagnostics."""
import hashlib
import json
from pathlib import Path
import shutil
import urllib.error
import urllib.request


def acquire(pin, archive, receipt):
    archive, receipt = Path(archive), Path(receipt)
    archive.parent.mkdir(parents=True, exist_ok=True)
    receipt.mkdir(parents=True, exist_ok=True)
    cached = archive.exists()
    candidate = archive if cached else archive.with_name(archive.name + '.part')
    report = dict(url=pin['url'], expected_sha256=pin['sha256'], cached=cached)
    record = receipt / (archive.name + '.acquisition.json')
    try:
        if not cached:
            candidate.unlink(missing_ok=True)
            try:
                response = urllib.request.urlopen(pin['url'], timeout=120)
            except urllib.error.HTTPError as error:
                response = error  # Preserve an HTTP error body too; never accept it.
            with response, candidate.open('wb') as output:
                report.update(effective_url=response.geturl(), status=response.status,
                              headers=dict(response.headers.items()))
                shutil.copyfileobj(response, output)
        digest = hashlib.sha256()
        size = 0
        with candidate.open('rb') as source:
            while chunk := source.read(1024 * 1024):
                digest.update(chunk); size += len(chunk)
        report.update(actual_sha256=digest.hexdigest(), bytes=size)
        if (not cached and report['status'] != 200) or digest.hexdigest() != pin['sha256']:
            raise RuntimeError(f"HTTP archive verification failed: {archive.name}; "
                               f"expected {pin['sha256']}, got {digest.hexdigest()}, {size} bytes; {record}")
        if not cached:
            candidate.replace(archive)  # Only verified bytes enter the source cache.
        report['result'] = 'verified'
        return archive
    except Exception as error:
        report.update(result='rejected', error=str(error))
        if candidate.is_file():
            rejected = receipt / (archive.name + '.rejected')
            shutil.copyfile(candidate, rejected)
            report['rejected_file'] = rejected.name
            report['actual_sha256'] = hashlib.sha256(rejected.read_bytes()).hexdigest()
            report['bytes'] = rejected.stat().st_size
        raise
    finally:
        record.write_text(json.dumps(report, indent=2) + '\n')
