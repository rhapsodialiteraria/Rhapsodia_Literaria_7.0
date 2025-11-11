import React from 'react';
import './Footer.css';
import footer_logo from '../../assets/footer_logo.svg';
import right_logo from '../../assets/right_logo.svg'; // <-- add your second logo here

const Footer = () => {
    return (
        <div className="footer">
            <div className="footer-top">
                <div className="footer-top-left">
                    <img src={footer_logo} alt="Footer Logo" className="footer-logo" />
                    <div className="footer-text">
                        <p>Department of English</p>
                        <p>St. Aloysius College, Edathua</p>
                    </div>
                </div>

                <div className="footer-top-right">
                    <img src={right_logo} alt="Right Logo" className="footer-right-logo" />
                </div>
            </div>

            <hr />

            <div className="footer-bottom">
                <p className="footer-bottom-left">
                    © 2025 St. Aloysius College, Edathua. All rights reserved.
                </p>
                <div className="footer-bottom-right">
                    <p>Terms of Service</p>
                    <p>Privacy Policy</p>
                    <p>Connect with us</p>
                </div>
            </div>
        </div>
    );
};

export default Footer;
