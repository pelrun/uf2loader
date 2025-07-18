import subprocess
import threading
import time
import os
import sys

# Try to use robot logger, fall back to standard logging
try:
    from robot.api import logger
except ImportError:
    import logging
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
    logger = logging.getLogger(__name__)

class RenodeTest:
    """A helper class to manage Renode simulation instances."""

    def __init__(self, renode_path, platform):
        self.renode_path = renode_path
        self.platform = platform
        self.process = None
        self.log_lines = []
        self.log_lock = threading.Lock()

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()

    def start(self):
        """Starts the Renode simulation."""
        renode_script = os.path.join(os.path.dirname(__file__), 'renode_script.resc')
        with open(renode_script, 'w') as f:
            f.write(f'$mach create\n')
            f.write(f'machine LoadPlatformDescription @{self.platform}\n')

        cmd = [os.path.join(self.renode_path, 'renode'), renode_script]
        self.process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        
        self.log_thread = threading.Thread(target=self._log_reader)
        self.log_thread.daemon = True
        self.log_thread.start()
        
        logger.info("Renode simulation started.")

    def stop(self):
        """Stops the Renode simulation."""
        if self.process:
            self.run_command('q') # Quit Renode
            self.process.terminate()
            self.process.wait()
            self.log_thread.join()
            logger.info("Renode simulation stopped.")

    def run_command(self, command):
        """Sends a command to the Renode monitor."""
        if self.process and self.process.poll() is None:
            self.process.stdin.write(command + '\n')
            self.process.stdin.flush()
            logger.info(f"Sent command to Renode: {command}")

    def uart_key(self, uart, key):
        """Sends a single character to the specified UART."""
        for char in key:
            self.run_command(f'{uart} WriteChar {ord(char)}')

    def wait_for_log_entry(self, entry, timeout=60):
        """Waits for a specific entry to appear in the Renode log."""
        start_time = time.time()
        while time.time() - start_time < timeout:
            with self.log_lock:
                for line in self.log_lines:
                    if entry in line:
                        logger.info(f"Found log entry: {entry}")
                        return True
            time.sleep(0.1)
        logger.error(f"Timeout waiting for log entry: {entry}")
        return False

    def _log_reader(self):
        """Reads log output from the Renode process."""
        for line in iter(self.process.stdout.readline, ''):
            with self.log_lock:
                self.log_lines.append(line)
            sys.stdout.write(line)
            sys.stdout.flush() 