'use strict';

const LEVELS = {
  off: 0,
  error: 1,
  warn: 2,
  info: 3,
  debug: 4,
  trace: 5,
};

/**
 * @param {boolean | string | undefined} value
 * @param {string | undefined} envValue
 * @returns {number}
 */
function parseLogLevel(value, envValue) {
  if (value === true) {
    return LEVELS.info;
  }
  if (value === false || value === undefined || value === null) {
    if (envValue !== undefined && envValue !== '') {
      return parseLogLevel(envValue, undefined);
    }
    return LEVELS.off;
  }
  if (typeof value === 'number') {
    if (value >= LEVELS.off && value <= LEVELS.trace) {
      return value;
    }
    return LEVELS.off;
  }
  if (typeof value === 'string') {
    const key = value.toLowerCase();
    if (Object.prototype.hasOwnProperty.call(LEVELS, key)) {
      return LEVELS[key];
    }
    return LEVELS.off;
  }
  return LEVELS.off;
}

/**
 * @param {number} level
 * @returns {string}
 */
function levelName(level) {
  for (const [name, value] of Object.entries(LEVELS)) {
    if (value === level) {
      return name;
    }
  }
  return 'off';
}

/**
 * @param {number} currentLevel
 * @param {number} messageLevel
 * @returns {boolean}
 */
function shouldLog(currentLevel, messageLevel) {
  return messageLevel <= currentLevel && currentLevel > LEVELS.off;
}

module.exports = {
  LEVELS,
  parseLogLevel,
  levelName,
  shouldLog,
};
