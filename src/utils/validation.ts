/**
 * Validates username format
 * - Max 20 alphanumeric characters
 * - Returns { valid: boolean, error?: string }
 */
export function validateUsername(username: string): { valid: boolean; error?: string } {
  if (!username || username.trim().length === 0) {
    return { valid: false, error: 'Username required' };
  }
  
  if (username.length > 20) {
    return { valid: false, error: 'Username must be 20 characters or less' };
  }
  
  if (!/^[a-zA-Z0-9_]+$/.test(username)) {
    return { valid: false, error: 'Username can only contain letters, numbers, and underscores' };
  }
  
  return { valid: true };
}

/**
 * Validates password format
 * - 8-20 characters
 * - At least one capital letter
 * - At least one number
 * - At least one special character (!@#$%^&*)
 * - Returns { valid: boolean, error?: string }
 */
export function validatePassword(password: string): { valid: boolean; error?: string } {
  if (!password) {
    return { valid: false, error: 'Password required' };
  }
  
  if (password.length < 8) {
    return { valid: false, error: 'Password must be at least 8 characters' };
  }
  
  if (password.length > 20) {
    return { valid: false, error: 'Password must be 20 characters or less' };
  }
  
  if (!/[A-Z]/.test(password)) {
    return { valid: false, error: 'Password must contain at least one uppercase letter' };
  }
  
  if (!/[0-9]/.test(password)) {
    return { valid: false, error: 'Password must contain at least one number' };
  }
  
  if (!/[!@#$%^&*]/.test(password)) {
    return { valid: false, error: 'Password must contain at least one special character (!@#$%^&*)' };
  }
  
  return { valid: true };
}

/**
 * Gets password requirement messages for UI display
 */
export function getPasswordRequirements(password: string): {
  minLength: boolean;
  maxLength: boolean;
  hasUppercase: boolean;
  hasNumber: boolean;
  hasSpecial: boolean;
} {
  return {
    minLength: password.length >= 8,
    maxLength: password.length <= 20,
    hasUppercase: /[A-Z]/.test(password),
    hasNumber: /[0-9]/.test(password),
    hasSpecial: /[!@#$%^&*]/.test(password),
  };
}
