import React from 'react';
import './Contact.css';
import theme_pattern from '../../assets/theme_pattern.svg';
import mail_icon from '../../assets/mail_icon.svg';
import location_icon from '../../assets/location_icon.svg';
import call_icon from '../../assets/call_icon.svg';

const Contact = () => {

    const onSubmit = async (event) => {
        event.preventDefault();
        const formData = new FormData(event.target);

        formData.append("access_key", "bc35d01f-7573-4ea4-817b-9ce622680969");

        const object = Object.fromEntries(formData);
        const json = JSON.stringify(object);

        const res = await fetch("https://api.web3forms.com/submit", {
            method: "POST",
            headers: {
                "Content-Type": "application/json",
                Accept: "application/json"
            },
            body: json
        }).then((res) => res.json());

        if (res.success) {
            alert(res.message);
        }
    };
    return (
        <div id='contact' className="contact">
            <div className="contact-title">
                <h1>Get in Touch</h1>
                <img src={theme_pattern} alt="" />
            </div>

            <div className="contact-section">
                <div className="contact-left">
                    <h1>Let’s Talk</h1>


                    <div className="contact-info">
                        <div className="contact-details">
                            <img src={mail_icon} alt="mail icon" />
                            <p>rhapsodialiteraria@gmail.com</p>
                        </div>
                        <div className="contact-details">
                            <img src={call_icon} alt="phone icon" />
                            <p>+91 8848274816</p>
                        </div>
                        <div className="contact-details">
                            <img src={location_icon} alt="location icon" />
                            <p>St. Aloysius College, Edathua, Kuttanad Taluk, Kerala</p>

                        </div>
                    </div>
                </div>
                <form onSubmit={onSubmit} className="contact-right">
                    <label htmlFor="">Your Name</label>
                    <input type="text" placeholder='Enter your name' name='name' />
                    <label htmlFor="">Your Email</label>
                    <input type="text" placeholder='Enter your email' name='email' />
                    <label htmlFor="">Write your message</label>
                    <textarea name="Message" rows='8' placeholder='Enter Your Message'></textarea>
                    <button type='submit' className="contact-submit">
                        Submit Now
                    </button>
                </form>
            </div>
        </div>
    );
};

export default Contact;
