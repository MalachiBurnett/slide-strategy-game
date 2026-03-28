import React, { useState } from 'react';
import { motion } from 'motion/react';
import { User, LogIn, UserPlus, Trophy, Lock, Users, Mail, ArrowLeft, CheckCircle, AlertCircle, Loader2 } from 'lucide-react';

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
}) => {
  const [showForgotPassword, setShowForgotPassword] = useState(false);
  const [forgotPasswordInput, setForgotPasswordInput] = useState('');
  const [forgotPasswordStatus, setForgotPasswordStatus] = useState<'idle' | 'loading' | 'success' | 'error'>('idle');
  const [forgotPasswordMessage, setForgotPasswordMessage] = useState('');
  const [showResendVerification, setShowResendVerification] = useState(false);
  const [resendVerificationInput, setResendVerificationInput] = useState('');
  const [resendVerificationStatus, setResendVerificationStatus] = useState<'idle' | 'loading' | 'success' | 'error'>('idle');
  const [resendVerificationMessage, setResendVerificationMessage] = useState('');

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
        <div className="flex justify-center mb-6">
          <div className="bg-[var(--primary)] p-4 rounded-2xl shadow-lg">
            <Trophy className="w-12 h-12 text-[var(--bg)]" />
          </div>
        </div>
        <h1 className="text-4xl font-black text-center mb-2 tracking-tight">SLIDE</h1>
        <p className="text-center opacity-40 mb-10 italic font-medium">A minimalist strategy game</p>
        
        <form onSubmit={handleAuth} className="space-y-4">
          <div className="relative">
            <User className="absolute left-4 top-4 w-5 h-5 text-[var(--primary)] opacity-60" />
            <input 
              type="text" 
              placeholder={authMode === 'login' ? "Username or Email" : "Username"} 
              className="w-full pl-12 pr-4 py-4 bg-[var(--bg)] bg-opacity-5 border border-[var(--primary)] border-opacity-10 rounded-2xl focus:ring-2 focus:ring-[var(--primary)] outline-none transition-all text-[var(--text)] font-bold placeholder:font-normal placeholder:opacity-40"
              value={username}
              onChange={(e) => setUsername(e.target.value)}
            />
          </div>
          {authMode === 'register' && (
            <div className="relative">
              <Mail className="absolute left-4 top-4 w-5 h-5 text-[var(--primary)] opacity-60" />
              <input 
                type="email" 
                placeholder="Email Address" 
                className="w-full pl-12 pr-4 py-4 bg-[var(--bg)] bg-opacity-5 border border-[var(--primary)] border-opacity-10 rounded-2xl focus:ring-2 focus:ring-[var(--primary)] outline-none transition-all text-[var(--text)] font-bold placeholder:font-normal placeholder:opacity-40"
                value={email}
                onChange={(e) => setEmail(e.target.value)}
              />
            </div>
          )}
          <div className="relative">
            <Lock className="absolute left-4 top-4 w-5 h-5 text-[var(--primary)] opacity-60" />
            <input 
              type="password" 
              placeholder="Password" 
              className="w-full pl-12 pr-4 py-4 bg-[var(--bg)] bg-opacity-5 border border-[var(--primary)] border-opacity-10 rounded-2xl focus:ring-2 focus:ring-[var(--primary)] outline-none transition-all text-[var(--text)] font-bold placeholder:font-normal placeholder:opacity-40"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
            />
          </div>
          
          <button type="submit" className="w-full py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black text-lg hover:opacity-90 transition-all flex items-center justify-center gap-3 shadow-xl shadow-[var(--primary)]/20 mt-6">
            {authMode === 'login' ? <LogIn className="w-6 h-6" /> : <UserPlus className="w-6 h-6" />}
            {authMode === 'login' ? 'Login' : 'Register'}
          </button>
        </form>

        <div className="mt-8 flex flex-col gap-4">
          {authMode === 'login' && (
            <>
              <button 
                onClick={() => setShowForgotPassword(true)}
                className="text-sm text-[var(--primary)] hover:underline text-center font-bold tracking-wide"
              >
                Forgot Password?
              </button>
              <button 
                onClick={() => setShowResendVerification(true)}
                className="text-sm text-[var(--primary)] hover:underline text-center font-bold tracking-wide"
              >
                Resend Verification Email?
              </button>
            </>
          )}
          <button 
            onClick={() => setAuthMode(authMode === 'login' ? 'register' : 'login')}
            className="text-sm text-[var(--primary)] hover:underline text-center font-bold tracking-wide uppercase"
          >
            {authMode === 'login' ? "Create an Account" : "Back to Login"}
          </button>
          <div className="flex items-center gap-4 my-2">
            <div className="flex-1 h-px bg-[var(--primary)] opacity-20"></div>
            <span className="text-xs opacity-40 uppercase font-black tracking-widest">OR</span>
            <div className="flex-1 h-px bg-[var(--primary)] opacity-20"></div>
          </div>
          <button 
            onClick={handleGuest}
            className="w-full py-4 bg-transparent border-2 border-[var(--primary)] text-[var(--primary)] rounded-2xl font-black hover:bg-[var(--primary)] hover:text-[var(--bg)] transition-all flex items-center justify-center gap-3"
          >
            <Users className="w-6 h-6" />
            Play as Guest
          </button>
        </div>
        
        {error && <p className="mt-4 text-red-500 text-center text-sm font-medium">{error}</p>}
      </motion.div>
    </div>
  );
};
