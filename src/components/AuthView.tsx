import React, { useState, useEffect } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { User, LogIn, UserPlus, Trophy, Lock, Users, Mail, ArrowLeft, CheckCircle, AlertCircle, Loader2, ChevronDown, HelpCircle } from 'lucide-react';
import { validateUsername, validatePassword, getPasswordRequirements, validateEmail } from '../utils/validation';

declare global {
  interface Window {
    google: any;
  }
}

interface AuthViewProps {
  authMode: 'login' | 'register';
  setAuthMode: (mode: 'login' | 'register') => void;
  username: string;
  setUsername: (value: string) => void;
  email: string;
  setEmail: (value: string) => void;
  password: string;
  setPassword: (value: string) => void;
  error: string;
  handleAuth: (e: React.FormEvent) => void;
  handleGuest: () => void;
  handleGoogleAuth: (credential: string) => void;
}

export const AuthView: React.FC<AuthViewProps> = ({
  authMode,
  setAuthMode,
  username,
  setUsername,
  email,
  setEmail,
  password,
  setPassword,
  error,
  handleAuth,
  handleGuest,
  handleGoogleAuth,
}) => {
  const [showForgotPassword, setShowForgotPassword] = useState(false);
  const [forgotPasswordInput, setForgotPasswordInput] = useState('');
  const [forgotPasswordStatus, setForgotPasswordStatus] = useState<'idle' | 'loading' | 'success' | 'error'>('idle');
  const [forgotPasswordMessage, setForgotPasswordMessage] = useState('');
  const [showResendVerification, setShowResendVerification] = useState(false);
  const [resendVerificationInput, setResendVerificationInput] = useState('');
  const [resendVerificationStatus, setResendVerificationStatus] = useState<'idle' | 'loading' | 'success' | 'error'>('idle');
  const [resendVerificationMessage, setResendVerificationMessage] = useState('');
  const [showOptions, setShowOptions] = useState(false);
  const [usernameError, setUsernameError] = useState<string>('');
  const [emailError, setEmailError] = useState<string>('');
  const [passwordError, setPasswordError] = useState<string>('');
  const [passwordRequirements, setPasswordRequirements] = useState(getPasswordRequirements(''));

  const clearFields = () => {
    setUsername('');
    setEmail('');
    setPassword('');
    setUsernameError('');
    setEmailError('');
    setPasswordError('');
  };

  const handleUsernameChange = (value: string) => {
    setUsername(value);
    if (value) {
      const validation = validateUsername(value);
      setUsernameError(validation.valid ? '' : validation.error || '');
    } else {
      setUsernameError('');
    }
  };

  const handleEmailChange = (value: string) => {
    setEmail(value);
    if (value) {
      const validation = validateEmail(value);
      setEmailError(validation.valid ? '' : validation.error || '');
    } else {
      setEmailError('');
    }
  };

  const handlePasswordChange = (value: string) => {
    setPassword(value);
    if (authMode === 'register') {
      const validation = validatePassword(value);
      setPasswordError(validation.valid ? '' : validation.error || '');
      setPasswordRequirements(getPasswordRequirements(value));
    } else {
      setPasswordError('');
    }
  };

  const handleForgotPasswordSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!forgotPasswordInput.trim()) return;
    
    setForgotPasswordStatus('loading');
    try {
      const res = await fetch('/api/request-password-reset', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ usernameOrEmail: forgotPasswordInput })
      });
      const data = await res.json();
      setForgotPasswordStatus('success');
      setForgotPasswordMessage(data.message || 'Check your email for a password reset link.');
    } catch (e) {
      setForgotPasswordStatus('error');
      setForgotPasswordMessage('Network error. Please try again.');
    }
  };

  const handleResendVerificationSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!resendVerificationInput.trim()) return;
    
    setResendVerificationStatus('loading');
    try {
      const res = await fetch('/api/resend-verification-email', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ usernameOrEmail: resendVerificationInput })
      });
      const data = await res.json();
      setResendVerificationStatus('success');
      setResendVerificationMessage(data.message || 'Check your email for a verification link.');
    } catch (e) {
      setResendVerificationStatus('error');
      setResendVerificationMessage('Network error. Please try again.');
    }
  };

  useEffect(() => {
    const handleCredentialResponse = (response: any) => {
      handleGoogleAuth(response.credential);
    };

    if (window.google && !showForgotPassword && !showResendVerification) {
      window.google.accounts.id.initialize({
        client_id: import.meta.env.VITE_GOOGLE_CLIENT_ID,
        callback: handleCredentialResponse
      });
      const parent = document.getElementById("googleSignInButton");
      if (parent) {
        window.google.accounts.id.renderButton(parent, {
          theme: "outline",
          size: "large",
          width: parent.offsetWidth || 384,
          shape: "pill",
          text: "signin_with",
          logo_alignment: "left"
        });
      }
    }
  }, [authMode, showForgotPassword, showResendVerification, handleGoogleAuth]);

  const faviconSvg = "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><defs><style>.wood-light { fill: %23decba4; } .wood-dark { fill: %23a67c52; } .wood-border { fill: %235d2e0a; } .wood-bg { fill: %235d2e0a; } .piece-black { fill: %232a2a2a; } .piece-border { stroke: %23333; stroke-width: 1.5; }</style></defs><rect width='100' height='100' rx='20' ry='20' class='wood-bg'/><rect x='8' y='8' width='84' height='84' rx='16' ry='16' class='wood-border'/><rect x='12' y='12' width='76' height='76' rx='12' ry='12' class='wood-light'/><circle cx='50' cy='50' r='24' class='piece-black piece-border'/><circle cx='50' cy='50' r='20' class='piece-black' opacity='0.95'/></svg>";


  if (showForgotPassword) {
    return (
      <div className="min-h-screen bg-[var(--bg)] flex items-center justify-center p-4 font-sans text-[var(--text)] transition-colors duration-500">
        <motion.div 
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-2xl w-full max-w-md border-b-8 border-[var(--accent)] border-opacity-50"
        >
          <button
            onClick={() => {
              setShowForgotPassword(false);
              setForgotPasswordInput('');
              setForgotPasswordStatus('idle');
              setForgotPasswordMessage('');
            }}
            className="flex items-center gap-2 text-[var(--primary)] hover:opacity-70 transition-opacity mb-6 font-bold"
          >
            <ArrowLeft className="w-5 h-5" />
            Back to Login
          </button>

          <div className="flex justify-center mb-6">
            <div className="bg-[var(--primary)] p-4 rounded-2xl shadow-lg">
              <Mail className="w-12 h-12 text-[var(--bg)]" />
            </div>
          </div>
          <h1 className="text-3xl font-black text-center mb-2">Reset Password</h1>
          <p className="text-center opacity-40 mb-10 italic font-medium">Enter your username or email to receive a reset link</p>
          
          {forgotPasswordStatus === 'success' ? (
            <div className="text-center space-y-4">
              <CheckCircle className="w-16 h-16 text-green-500 mx-auto" />
              <p className="font-bold text-lg">{forgotPasswordMessage}</p>
              <button
                onClick={() => {
                  setShowForgotPassword(false);
                  setForgotPasswordInput('');
                  setForgotPasswordStatus('idle');
                  setForgotPasswordMessage('');
                }}
                className="w-full py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black text-lg hover:opacity-90 transition-all"
              >
                Return to Login
              </button>
            </div>
          ) : (
            <form onSubmit={handleForgotPasswordSubmit} className="space-y-4">
              <div className="relative">
                <User className="absolute left-4 top-4 w-5 h-5 text-[var(--primary)] opacity-60" />
                <input 
                  type="text" 
                  placeholder="Username or Email" 
                  className="w-full pl-12 pr-4 py-4 bg-[var(--bg)] bg-opacity-5 border border-[var(--primary)] border-opacity-10 rounded-2xl focus:ring-2 focus:ring-[var(--primary)] outline-none transition-all text-[var(--text)] font-bold placeholder:font-normal placeholder:opacity-40"
                  value={forgotPasswordInput}
                  onChange={(e) => setForgotPasswordInput(e.target.value)}
                />
              </div>
              
              <button 
                type="submit" 
                disabled={forgotPasswordStatus === 'loading' || !forgotPasswordInput.trim()}
                className="w-full py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black text-lg hover:opacity-90 transition-all flex items-center justify-center gap-3 shadow-xl shadow-[var(--primary)]/20 mt-6 disabled:opacity-50"
              >
                {forgotPasswordStatus === 'loading' && <Loader2 className="w-6 h-6 animate-spin" />}
                <Mail className="w-6 h-6" />
                Send Reset Link
              </button>

              {forgotPasswordStatus === 'error' && (
                <div className="mt-4 p-4 bg-red-500 bg-opacity-10 rounded-2xl flex items-center gap-2">
                  <AlertCircle className="w-5 h-5 text-red-500 flex-shrink-0" />
                  <p className="text-red-600 text-sm font-medium">{forgotPasswordMessage}</p>
                </div>
              )}
            </form>
          )}
        </motion.div>
      </div>
    );
  }

  if (showResendVerification) {
    return (
      <div className="min-h-screen bg-[var(--bg)] flex items-center justify-center p-4 font-sans text-[var(--text)] transition-colors duration-500">
        <motion.div 
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-2xl w-full max-w-md border-b-8 border-[var(--accent)] border-opacity-50"
        >
          <button
            onClick={() => {
              setShowResendVerification(false);
              setResendVerificationInput('');
              setResendVerificationStatus('idle');
              setResendVerificationMessage('');
            }}
            className="flex items-center gap-2 text-[var(--primary)] hover:opacity-70 transition-opacity mb-6 font-bold"
          >
            <ArrowLeft className="w-5 h-5" />
            Back to Login
          </button>

          <div className="flex justify-center mb-6">
            <div className="bg-[var(--primary)] p-4 rounded-2xl shadow-lg">
              <Mail className="w-12 h-12 text-[var(--bg)]" />
            </div>
          </div>
          <h1 className="text-3xl font-black text-center mb-2">Resend Verification Email</h1>
          <p className="text-center opacity-40 mb-10 italic font-medium">Enter your username or email to receive a verification link</p>
          
          {resendVerificationStatus === 'success' ? (
            <div className="text-center space-y-4">
              <CheckCircle className="w-16 h-16 text-green-500 mx-auto" />
              <p className="font-bold text-lg">{resendVerificationMessage}</p>
              <button
                onClick={() => {
                  setShowResendVerification(false);
                  setResendVerificationInput('');
                  setResendVerificationStatus('idle');
                  setResendVerificationMessage('');
                }}
                className="w-full py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black text-lg hover:opacity-90 transition-all"
              >
                Return to Login
              </button>
            </div>
          ) : (
            <form onSubmit={handleResendVerificationSubmit} className="space-y-4">
              <div className="relative">
                <User className="absolute left-4 top-4 w-5 h-5 text-[var(--primary)] opacity-60" />
                <input 
                  type="text" 
                  placeholder="Username or Email" 
                  className="w-full pl-12 pr-4 py-4 bg-[var(--bg)] bg-opacity-5 border border-[var(--primary)] border-opacity-10 rounded-2xl focus:ring-2 focus:ring-[var(--primary)] outline-none transition-all text-[var(--text)] font-bold placeholder:font-normal placeholder:opacity-40"
                  value={resendVerificationInput}
                  onChange={(e) => setResendVerificationInput(e.target.value)}
                />
              </div>
              
              <button 
                type="submit" 
                disabled={resendVerificationStatus === 'loading' || !resendVerificationInput.trim()}
                className="w-full py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black text-lg hover:opacity-90 transition-all flex items-center justify-center gap-3 shadow-xl shadow-[var(--primary)]/20 mt-6 disabled:opacity-50"
              >
                {resendVerificationStatus === 'loading' && <Loader2 className="w-6 h-6 animate-spin" />}
                <Mail className="w-6 h-6" />
                Send Verification Email
              </button>

              {resendVerificationStatus === 'error' && (
                <div className="mt-4 p-4 bg-red-500 bg-opacity-10 rounded-2xl flex items-center gap-2">
                  <AlertCircle className="w-5 h-5 text-red-500 flex-shrink-0" />
                  <p className="text-red-600 text-sm font-medium">{resendVerificationMessage}</p>
                </div>
              )}
            </form>
          )}
        </motion.div>
      </div>
    );
  }


  return (
    <div className="min-h-screen bg-[var(--bg)] flex items-center justify-center p-4 font-sans text-[var(--text)] transition-colors duration-500">
      <motion.div 
        initial={{ opacity: 0, y: 20 }}
        animate={{ opacity: 1, y: 0 }}
        className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-2xl w-full max-w-md border-b-8 border-[var(--accent)] border-opacity-50"
      >
        <div className="flex items-center justify-center gap-4 mb-8">
          <img src={faviconSvg} alt="Logo" className="w-12 h-12 shadow-lg rounded-xl" />
          <h1 className="text-4xl font-black tracking-tighter">Slide</h1>
        </div>
        
        <form onSubmit={handleAuth} className="space-y-4">
          <div className="relative">
            <User className="absolute left-4 top-4 w-5 h-5 text-[var(--primary)] opacity-60" />
            <input 
              type="text" 
              placeholder={authMode === 'login' ? "Username or Email" : "Username"} 
              className={`w-full pl-12 pr-4 py-4 bg-[var(--bg)] bg-opacity-5 border rounded-2xl focus:ring-2 outline-none transition-all text-[var(--text)] font-bold placeholder:font-normal placeholder:opacity-40 ${
                usernameError ? 'border-red-500 focus:ring-red-500' : 'border-[var(--primary)] border-opacity-10 focus:ring-[var(--primary)]'
              }`}
              value={username}
              onChange={(e) => handleUsernameChange(e.target.value)}
            />
            {usernameError && (
              <p className="mt-2 text-xs text-red-500 font-medium flex items-center gap-1">
                <AlertCircle className="w-3 h-3" />
                {usernameError}
              </p>
            )}
            {authMode === 'register' && username && !usernameError && (
              <p className="mt-2 text-xs text-green-500 font-medium flex items-center gap-1">
                <CheckCircle className="w-3 h-3" />
                Username looks good
              </p>
            )}
          </div>
          {authMode === 'register' && (
            <div className="relative">
              <Mail className="absolute left-4 top-4 w-5 h-5 text-[var(--primary)] opacity-60" />
              <input 
                type="email" 
                placeholder="Email Address" 
                className={`w-full pl-12 pr-4 py-4 bg-[var(--bg)] bg-opacity-5 border rounded-2xl focus:ring-2 outline-none transition-all text-[var(--text)] font-bold placeholder:font-normal placeholder:opacity-40 ${
                  emailError ? 'border-red-500 focus:ring-red-500' : 'border-[var(--primary)] border-opacity-10 focus:ring-[var(--primary)]'
                }`}
                value={email}
                onChange={(e) => handleEmailChange(e.target.value)}
              />
              {emailError && (
                <p className="mt-2 text-xs text-red-500 font-medium flex items-center gap-1">
                  <AlertCircle className="w-3 h-3" />
                  {emailError}
                </p>
              )}
            </div>
          )}
          <div className="relative">
            <Lock className="absolute left-4 top-4 w-5 h-5 text-[var(--primary)] opacity-60" />
            <input 
              type="password" 
              placeholder="Password" 
              className={`w-full pl-12 pr-4 py-4 bg-[var(--bg)] bg-opacity-5 border rounded-2xl focus:ring-2 outline-none transition-all text-[var(--text)] font-bold placeholder:font-normal placeholder:opacity-40 ${
                passwordError ? 'border-red-500 focus:ring-red-500' : 'border-[var(--primary)] border-opacity-10 focus:ring-[var(--primary)]'
              }`}
              value={password}
              onChange={(e) => handlePasswordChange(e.target.value)}
            />
            {authMode === 'register' && password && (
              <div className="mt-3 p-3 bg-[var(--bg)] rounded-lg border border-[var(--primary)] border-opacity-20 space-y-2">
                <p className="text-xs font-bold opacity-60 uppercase tracking-wide">Password Requirements:</p>
                <div className="space-y-1 text-xs">
                  <div className={`flex items-center gap-2 ${passwordRequirements.minLength ? 'text-green-500' : 'text-red-400'}`}>
                    {passwordRequirements.minLength ? <CheckCircle className="w-3 h-3" /> : <AlertCircle className="w-3 h-3" />}
                    At least 8 characters
                  </div>
                  <div className={`flex items-center gap-2 ${passwordRequirements.maxLength ? 'text-green-500' : 'text-red-400'}`}>
                    {passwordRequirements.maxLength ? <CheckCircle className="w-3 h-3" /> : <AlertCircle className="w-3 h-3" />}
                    Maximum 20 characters
                  </div>
                  <div className={`flex items-center gap-2 ${passwordRequirements.hasUppercase ? 'text-green-500' : 'text-red-400'}`}>
                    {passwordRequirements.hasUppercase ? <CheckCircle className="w-3 h-3" /> : <AlertCircle className="w-3 h-3" />}
                    One uppercase letter (A-Z)
                  </div>
                  <div className={`flex items-center gap-2 ${passwordRequirements.hasNumber ? 'text-green-500' : 'text-red-400'}`}>
                    {passwordRequirements.hasNumber ? <CheckCircle className="w-3 h-3" /> : <AlertCircle className="w-3 h-3" />}
                    One number (0-9)
                  </div>
                  <div className={`flex items-center gap-2 ${passwordRequirements.hasSpecial ? 'text-green-500' : 'text-red-400'}`}>
                    {passwordRequirements.hasSpecial ? <CheckCircle className="w-3 h-3" /> : <AlertCircle className="w-3 h-3" />}
                    One special character (!@#$%^&*)
                  </div>
                </div>
              </div>
            )}
          </div>
          
          <button 
            type="submit" 
            disabled={(authMode === 'register' && (!!usernameError || !!passwordError || !!emailError || !username || !password || !email)) || (authMode === 'login' && (!username || !password))}
            className="w-full py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black text-lg hover:opacity-90 transition-all flex items-center justify-center gap-3 shadow-xl shadow-[var(--primary)]/20 mt-6 disabled:opacity-50"
          >
            {authMode === 'login' ? <LogIn className="w-6 h-6" /> : <UserPlus className="w-6 h-6" />}
            {authMode === 'login' ? 'Login' : 'Register'}
          </button>
        </form>

        <div className="mt-6 flex flex-col gap-4">
          {authMode === 'login' ? (
            <>
              <button 
                onClick={() => { clearFields(); setAuthMode('register'); }}
                className="w-full py-4 bg-transparent border-2 border-[var(--primary)] text-[var(--primary)] rounded-2xl font-black text-lg hover:bg-[var(--primary)] hover:text-[var(--bg)] transition-all flex items-center justify-center gap-3"
              >
                <UserPlus className="w-6 h-6" />
                Create Account
              </button>

              <div id="googleSignInButton" className="w-full h-[56px] overflow-hidden rounded-2xl flex justify-center border-2 border-[var(--primary)] border-opacity-10 shadow-sm hover:border-opacity-30 transition-all"></div>

              <div className="flex items-center gap-4 py-2">
                <div className="flex-1 h-px bg-[var(--primary)] opacity-10"></div>
                <span className="text-[10px] opacity-30 uppercase font-black tracking-widest">ALTERNATIVE</span>
                <div className="flex-1 h-px bg-[var(--primary)] opacity-10"></div>
              </div>

              <button 
                onClick={handleGuest}
                className="w-full py-4 bg-transparent border-2 border-[var(--primary)] text-[var(--primary)] rounded-2xl font-black text-lg hover:bg-[var(--primary)] hover:text-[var(--bg)] transition-all flex items-center justify-center gap-3"
              >
                <Users className="w-6 h-6" />
                Play as Guest
              </button>

              <div className="relative">
                <button 
                  onClick={() => setShowOptions(!showOptions)}
                  className="w-full py-3 flex items-center justify-center gap-2 text-sm text-[var(--primary)] font-bold opacity-60 hover:opacity-100 transition-all"
                >
                  <HelpCircle className="w-4 h-4" />
                  Can't log in?
                  <motion.div animate={{ rotate: showOptions ? 180 : 0 }}>
                    <ChevronDown className="w-4 h-4" />
                  </motion.div>
                </button>
                
                <AnimatePresence>
                  {showOptions && (
                    <motion.div 
                      initial={{ opacity: 0, height: 0 }}
                      animate={{ opacity: 1, height: 'auto' }}
                      exit={{ opacity: 0, height: 0 }}
                      className="overflow-hidden flex flex-col items-center gap-2 pt-2 pb-4"
                    >
                      <button 
                        onClick={() => setShowForgotPassword(true)}
                        className="text-sm text-[var(--primary)] hover:underline font-bold"
                      >
                        Reset Password
                      </button>
                      <button 
                        onClick={() => setShowResendVerification(true)}
                        className="text-sm text-[var(--primary)] hover:underline font-bold"
                      >
                        Resend Verification Email
                      </button>
                    </motion.div>
                  )}
                </AnimatePresence>
              </div>
            </>
          ) : (
            <button 
              onClick={() => { clearFields(); setAuthMode('login'); }}
              className="text-sm text-[var(--primary)] hover:underline text-center font-bold tracking-wide uppercase mt-4"
            >
              Back to Login
            </button>
          )}
        </div>
        
        {error && <p className="mt-4 text-red-500 text-center text-sm font-medium">{error}</p>}
      </motion.div>
    </div>
  );
};
