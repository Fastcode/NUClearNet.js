const path = require('path');
const { EOL } = require('os');
const { existsSync } = require('fs');
const { spawn } = require('child_process');

const workspace = process.env.GITHUB_WORKSPACE;
const npmCommand = process.platform === 'win32' ? 'npm.cmd' : 'npm';

(async () => {
  const event = process.env.GITHUB_EVENT_PATH ? require(process.env.GITHUB_EVENT_PATH) : {};

  console.log('release event action:', event.action);

  if (event.action !== 'published' || !event.release) {
    exitSuccess('This action is only run when a release is published.');
    return;
  }

  console.log('release tag name:', event.release.tag_name);

  if (!event.release.tag_name) {
    exitSuccess('This action is only run when a release is published with a tag name.');
    return;
  }

  const releaseTargetBranch = event.release.target_commitish;

  console.log('release target branch:', releaseTargetBranch);

  if (releaseTargetBranch !== 'main') {
    exitSuccess('This action is only run when a release is published from the main branch.');
    return;
  }

  const releaseVersion = event.release.tag_name.startsWith('v')
    ? event.release.tag_name.slice(1)
    : event.release.tag_name;

  console.log('release version:', releaseVersion);

  const pkg = getPackageJson();
  const packageVersion = pkg.version.toString();

  console.log('package.json version:', packageVersion);

  if (packageVersion !== releaseVersion) {
    exitFailure('The version in package.json and the release tag version do not match.');
    return;
  }

  try {
    console.log('publishing package...');

    await runInWorkspace(npmCommand, ['publish']);

    exitSuccess(`Package version v${releaseVersion} published!`);
  } catch (e) {
    logError(e);
    exitFailure('Failed to publish package');
    return;
  }
})();

function getPackageJson() {
  const pathToPackage = path.join(workspace, 'package.json');
  if (!existsSync(pathToPackage)) throw new Error("package.json could not be found in your project's root.");
  return require(pathToPackage);
}

function exitSuccess(message) {
  console.info(`✔  success   ${message}`);
  process.exit(0);
}

function exitFailure(message) {
  logError(message);
  process.exit(1);
}

function logError(error) {
  console.error(`✖  fatal     ${error.stack || error}`);
}

function runInWorkspace(command, args) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, { cwd: workspace });

    const errorMessages = [];
    let isDone = false;

    child.on('error', (error) => {
      if (!isDone) {
        isDone = true;
        reject(error);
      }
    });

    child.stderr.on('data', (chunk) => errorMessages.push(chunk));

    child.on('exit', (code) => {
      if (!isDone) {
        if (code === 0) {
          resolve();
        } else {
          reject(`${errorMessages.join('')}${EOL}${command} exited with code ${code}`);
        }
      }
    });
  });
}
