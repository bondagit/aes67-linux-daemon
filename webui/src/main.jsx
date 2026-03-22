import React from 'react';
import { createRoot } from 'react-dom/client';
import { BrowserRouter } from 'react-router-dom';
import { ToastContainer } from 'react-toastify';
import 'react-toastify/dist/ReactToastify.css';
import './styles/theme.css';
import App from './App';

createRoot(document.getElementById('root')).render(
  <BrowserRouter>
    <App />
    <ToastContainer position="bottom-right" theme="dark" />
  </BrowserRouter>
);
